# Voice Stick

Voice Stick turns an M5Stack StickS3 or M5StickC Plus into a Bluetooth push-to-talk input device for macOS.

Hold the front button on the StickS3 to record. When you release it, the macOS menu bar app sends the audio to ASR, shows the recognized text, and pastes the final result into the currently focused input field after a short confirmation countdown. By default it pastes text and presses Return; `auto_enter` can be disabled in settings.

## Project Layout

- `firmware/`: ESP-IDF firmware for M5Stack StickS3 / ESP32-S3 and M5StickC Plus / ESP32.
- `desktop/macos/`: Swift Package for the native macOS menu bar app.
- `desktop/windows/`: Windows desktop app workspace.
- `desktop/linux/`: Linux desktop app workspace.
- `docs/protocol.md`: BLE protocol between StickS3 and macOS.
- `docs/volcengine-asr.md`: trimmed Volcengine ASR notes used by the desktop client.
- `scripts/`: sprite slicing, palette tuning, and LVGL ARGB binary conversion helpers.

## Current Features

- Firmware advertises as `VS-XXXX`, where `XXXX` is derived from the last two bytes of the eFuse MAC.
- The macOS app only connects to paired `VS-XXXX` devices and can keep multiple paired devices connected at once. The menu bar app lists every paired device and shows whether each one is connected or still scanning.
- The front button maps to the protocol `primary` role; it starts a recording session on press and ends it on release when the app has put the device in `ready`.
- The firmware reads 16 kHz mono PCM from the ES8311 microphone, encodes it as Opus, and sends it over BLE notifications.
- The macOS app wraps incoming Opus payloads into Ogg Opus and forwards them to ASR over WebSocket.
- ASR providers can be direct Volcengine or VoiceStick Cloud relay.
- During recognition, the macOS app shows a floating overlay and menu bar status. The firmware display stays in the thinking state after button release until the text is pasted or cancelled.
- Final text enters a 1.2 second confirmation countdown.
- Pressing the front button during the countdown pauses auto-paste. Pressing the front button again confirms paste; pressing the side button cancels it.
- Pressing the side button while idle restores the last recoverable input confirmation.
- Optional debug audio cache saves each valid recognition session as Ogg Opus, with the source device ID included in the file name when available.
- Firmware updates are checked from a signed-by-hash manifest on app launch, device connect/reconnect, and manual menu refresh. Updates are offered per connected device.
- The firmware screen shows pairing, ready, listening, thinking, pending confirmation, error, and battery states based on app-sent `ui_state` updates. It dims after 30 seconds of inactivity. On battery power it enters deep sleep after 5 minutes; while charging or USB powered it stays at the dimmed-screen stage. The front button wakes it from deep sleep.

## User Manual

See `docs/user-manual.md` for Windows setup, pairing, output modes, M5StickC
Plus flashing, WeChat voice input, and troubleshooting.

## Hardware Targets

### M5Stack StickS3

- Board: M5Stack StickS3 / ESP32-S3-PICO-1-N8R8
- Front button: GPIO11, protocol `primary`, push-to-talk and deep-sleep wake
- Side button: GPIO12, protocol `secondary`, cancel or restore the last input confirmation
- PMIC IRQ: GPIO13
- Audio codec: ES8311 over I2S, 16 kHz / 16 bit / mono
- Display: 135 x 240 ST7789P3 portrait screen
- LCD backlight: GPIO38 PWM

### M5Stack M5StickC Plus

- Board: M5Stack M5StickC Plus / ESP32-PICO-D4, 4 MB flash
- Button A: GPIO37, protocol `primary`, push-to-talk and deep-sleep wake
- Button B: GPIO39, protocol `secondary`, cancel or restore the last input confirmation
- PMU: AXP192 on I2C SDA GPIO21 / SCL GPIO22
- Audio: SPM1423 PDM microphone on CLK GPIO0 / DATA GPIO34
- Display: 135 x 240 ST7789v2 portrait screen
- LCD pins: MOSI GPIO15, SCLK GPIO13, DC GPIO23, RST GPIO18, CS GPIO5
- LCD power/backlight: AXP192 LDO rails

Board definitions live in `firmware/components/voice_board`.

## Interaction Model

| State | Front button | Side button |
| --- | --- | --- |
| Unpaired / disconnected | No recording; screen shows `VS-XXXX` | No effective action |
| Connected idle | Hold to record | Restore last input confirmation |
| Recording | Release to finish recording | Does not cancel the active recording |
| Thinking / finalizing | New recording is ignored | Cancel the in-progress recognition |
| Pending confirmation countdown | Pause auto-paste and keep pending confirmation | Cancel pending text |
| Manual pending confirmation | Confirm paste | Cancel pending text |

The firmware reports raw button facts (`button_down` / `button_up` with
`primary` or `secondary`). The macOS app owns the interaction state machine and
sends `ui_state` updates back to the firmware for the screen.

By default the paste flow presses Return after paste. Disable `Press Return after paste` in settings, or set `auto_enter = false` in the config file, to paste without sending Return.

## Audio Path

```text
StickS3 mic -> ES8311/I2S PCM -> Opus -> BLE -> macOS -> Ogg Opus -> ASR -> paste
M5StickC Plus mic -> SPM1423/PDM PCM -> Opus -> BLE -> macOS -> Ogg Opus -> ASR -> paste
```

The desktop app does not decode Opus back to PCM for ASR. It forwards Ogg Opus directly.

## BLE Protocol Summary

GATT service:

```text
8f2f0b84-6e6f-4b23-88f7-3a3ceafc5100
```

Characteristics:

| Name | UUID | Direction | Properties |
| --- | --- | --- | --- |
| `audio_tx` | `8f2f0b84-6e6f-4b23-88f7-3a3ceafc5101` | StickS3 -> Mac | notify |
| `state_tx` | `8f2f0b84-6e6f-4b23-88f7-3a3ceafc5102` | StickS3 -> Mac | notify |
| `control_rx` | `8f2f0b84-6e6f-4b23-88f7-3a3ceafc5103` | Mac -> StickS3 | write without response |

See `docs/protocol.md` for the full frame format.

## Firmware Build

Prepare ESP-IDF. The commands below use the local path `~/esp/v5.5.1/esp-idf`; replace it if your ESP-IDF checkout lives elsewhere.

StickS3 default build:

```sh
cd firmware
. "$HOME/esp/v5.5.1/esp-idf/export.sh"
idf.py set-target esp32s3
idf.py build
```

M5StickC Plus local build:

```sh
cd firmware
. "$HOME/esp/v5.5.1/esp-idf/export.sh"
idf.py -B build_m5stickc_plus -D SDKCONFIG=sdkconfig.m5stickc_plus -D SDKCONFIG_DEFAULTS=sdkconfig.defaults.m5stickc_plus set-target esp32
idf.py -B build_m5stickc_plus -D SDKCONFIG=sdkconfig.m5stickc_plus -D SDKCONFIG_DEFAULTS=sdkconfig.defaults.m5stickc_plus build
```

M5StickC Plus audio hardware self-test build:

```sh
idf.py -B build_m5stickc_plus_selftest -D SDKCONFIG=sdkconfig.m5stickc_plus_selftest -D "SDKCONFIG_DEFAULTS=sdkconfig.defaults.m5stickc_plus;sdkconfig.defaults.m5stickc_plus.selftest" set-target esp32
idf.py -B build_m5stickc_plus_selftest -D SDKCONFIG=sdkconfig.m5stickc_plus_selftest -D "SDKCONFIG_DEFAULTS=sdkconfig.defaults.m5stickc_plus;sdkconfig.defaults.m5stickc_plus.selftest" build
```

The self-test image reads a short burst from the board microphone at boot and
logs `audio self-test ok` with basic PCM statistics. It is for bench testing
new board ports and is not used for normal firmware builds.

M5StickC Plus BLE audio end-to-end diagnostic build:

```sh
idf.py -B build_m5stickc_plus_ble_audio_e2e -D SDKCONFIG=sdkconfig.m5stickc_plus_ble_audio_e2e -D "SDKCONFIG_DEFAULTS=sdkconfig.defaults.m5stickc_plus;sdkconfig.defaults.m5stickc_plus.ble_audio_e2e" set-target esp32
idf.py -B build_m5stickc_plus_ble_audio_e2e -D SDKCONFIG=sdkconfig.m5stickc_plus_ble_audio_e2e -D "SDKCONFIG_DEFAULTS=sdkconfig.defaults.m5stickc_plus;sdkconfig.defaults.m5stickc_plus.ble_audio_e2e" build
```

The e2e diagnostic image waits for the desktop app to subscribe to BLE state and
audio notifications, then simulates one short primary-button hold through the
normal firmware event path. It is for bench testing only; flash the normal
`sdkconfig.defaults.m5stickc_plus` image again after running it. On Windows, do
not pair `VS-XXXX` through Settings > Bluetooth > Add device; use the
VoiceStick app pairing/autoconnect path so the custom GATT service is discovered
without leaving stale Windows pairing records.

M5StickC Plus bring-up notes:

- Use the VoiceStick app pairing/autoconnect path on Windows. Windows Settings
  Bluetooth pairing can leave stale records and make WinRT GATT discovery fail.
- If GATT service discovery repeatedly fails on Windows, remove stale `VS-XXXX`
  Bluetooth records and restart Bluetooth before retrying from the app.
- The tested Windows flash path was reliable at 115200 baud. Faster baud rates
  may work on other machines, but 115200 is the safe fallback.
- Button A/B are input-only GPIOs with external pull-ups; do not enable internal
  pulls for GPIO37/GPIO39.
- The C Plus LCD backlight depends on AXP192 LDO rail setup. A low LDO2 level
  can make the screen appear uninitialized even when SPI display init succeeded.
- C Plus currently uses a USB-flashed 4 MB factory image, not the StickS3 BLE
  OTA partition layout.

If `export.sh` reports that the ESP-IDF Python virtual environment is missing, run the matching installer once:

```sh
"$HOME/esp/v5.5.1/esp-idf/install.sh" esp32s3
```

Flash and monitor:

```sh
idf.py -p /dev/cu.usbmodemXXXX flash monitor
```

M5StickC Plus USB flash uses its 4 MB factory partition table:

```sh
idf.py -B build_m5stickc_plus -D SDKCONFIG=sdkconfig.m5stickc_plus -D SDKCONFIG_DEFAULTS=sdkconfig.defaults.m5stickc_plus -p /dev/cu.usbserial-XXXX erase-flash flash monitor
```

StickC-Plus supported download baud rates include 115200, 250000, 500000, 750000, and 1500000 bps. On Windows the device may require the FTDI driver before it appears as a serial port.

The firmware uses an OTA partition table with two 3 MB app slots plus a reserved 1984 KB `storage` partition. Devices flashed with the old single-app table need one USB flash to install the new partition table before BLE OTA updates can be used:

```sh
idf.py -p /dev/cu.usbmodemXXXX erase-flash flash monitor
```

M5StickC Plus currently builds as a USB-flashed factory image, not a BLE OTA release image. The 4 MB flash does not fit the existing two 3 MB OTA-slot layout.

Firmware dependencies are declared through the ESP-IDF component manager:

- `espressif/button`
- `espressif/esp_codec_dev`
- `78/esp-opus`
- `lvgl/lvgl`

## macOS Desktop Build

The desktop app is a Swift Package targeting macOS 12 or newer.

```sh
cd desktop/macos
swift build
```

Run it:

```sh
swift run VoiceStickApp
```

The app is a menu bar accessory app and requests Bluetooth permission. Text insertion uses simulated `Command-V` plus optional Return. If macOS blocks the keyboard events, grant Accessibility permission to the running terminal or app in System Settings.

For a distributable macOS release with Sparkle updates:

```sh
SPARKLE_PUBLIC_ED_KEY="..." scripts/build-macos.sh --release
scripts/make-dmg.sh
```

The build script writes `build/VoiceStick-<version>.app`, `build/VoiceStick-<version>.zip`, and a Sparkle signature file. Upload the DMG and ZIP to GitHub Releases, then update `website/appcast.xml` for the GitHub Pages update feed.

For a distributable Windows release with WinSparkle updates, the MSI is the update package. The Windows signing certificate is expected to live on the local signing machine, such as a USB hardware key:

```bat
scripts\build-msi.bat
```

The script signs `VoiceStick.exe`, `WinSparkle.dll`, and `VoiceStick_<version>.msi` locally. Upload that MSI to the matching GitHub Release, then manually run the `Deploy Website to GitHub Pages` workflow so the shared appcast points Windows clients at the Release asset URL.

GitHub Actions can do the macOS and firmware release path automatically when a `v<version>` tag is pushed. The tag must match `VERSION`, for example `VERSION=0.2.1` pairs with `v0.2.1`. The release workflow publishes the macOS DMG/ZIP/signature and firmware assets to GitHub Releases, then deploys the website/appcast to GitHub Pages. The Windows MSI is uploaded afterward from the local signing machine. See `docs/release.md` for the full release process, including the Windows-first and Windows-afterward flows.

The same release workflow also builds the StickS3 firmware with ESP-IDF v5.5.1 and uploads firmware artifacts to Aliyun OSS:

| File | Use |
| --- | --- |
| `voicestick-firmware-sticks3-ota-<version>.bin` | BLE OTA image used by the macOS app |
| `voicestick-firmware-sticks3-merged-<version>.bin` | Browser/USB flashing image written at offset `0x0` |
| `manifest.json` | Latest firmware metadata for the app and website |

Artifacts are published under both the versioned directory and `latest`:

```text
voicestick/firmwares/<version>/manifest.json
voicestick/firmwares/latest/manifest.json
```

The macOS app checks the stable latest manifest URL on app launch, on device connect/reconnect, and at most once every 24 hours while running. The menu also has `Check for Firmware Updates` for a manual refresh. If a connected device reports an older `firmware_version` than the manifest version, its device submenu shows `Update to <version>...`. The app downloads the manifest `ota_url` and verifies `ota_size` plus `ota_sha256` before starting BLE OTA.

Configure these GitHub secrets before running the release workflow:

| Name | Description |
| --- | --- |
| `ALIYUN_OSS_ACCESS_KEY_ID` | OSS upload access key ID |
| `ALIYUN_OSS_ACCESS_KEY_SECRET` | OSS upload access key secret |
| `ALIYUN_OSS_ENDPOINT` | OSS endpoint, for example `https://oss-cn-hangzhou.aliyuncs.com` |
| `ALIYUN_OSS_BUCKET` | OSS bucket name |

Set the repository variable `ALIYUN_OSS_PUBLIC_BASE_URL` to the public OSS base URL, for example `https://xiaozhi-voice-assistant.oss-cn-shenzhen.aliyuncs.com`. Optional variable `ALIYUN_OSS_PREFIX` controls the OSS object prefix and defaults to `voicestick/firmwares`.

## Local Config

Config path:

```text
~/Library/Application Support/VoiceStick/config.toml
```

Create it from the example:

```sh
mkdir -p "$HOME/Library/Application Support/VoiceStick"
cp desktop/macos/Config/config.example.toml "$HOME/Library/Application Support/VoiceStick/config.toml"
```

Example:

```toml
asr_provider = "volcengine"
voicestick_api_key = ""
voicestick_cloud_url = "wss://api.xiaozhi.me/voicestick/asr/"
volcengine_api_key = "your_volcengine_asr_api_key"
llm_base_url = "https://api.openai.com/v1"
llm_api_key = "your_openai_compatible_llm_api_key"
llm_model = "gpt-5.5"
interaction_mode = "hold_to_talk"
resource_id = "volc.seedasr.sauc.duration"
asr_hotwords = "小智,VoiceStick"
paired_device_ids = ""
device_theme_colors = ""
device_overlay_positions = ""
auto_enter = true
debug_audio_cache = false
# debug_audio_dir = "~/Library/Application Support/VoiceStick/DebugAudio"

[output]
target = "focused_app"
transform = "original"
translation_target = "en"

# Optional per-device subtitle translation:
# [device.C3D8.output]
# transform = "translate"
# translation_target = "en"
```

Fields:

| Field | Description |
| --- | --- |
| `asr_provider` | `volcengine` or `voicestick_cloud` |
| `volcengine_api_key` | Direct Volcengine API key, sent as `X-Api-Key` |
| `voicestick_api_key` | VoiceStick Cloud relay API key, sent as `X-Api-Key` |
| `voicestick_cloud_url` | Cloud relay WebSocket URL |
| `llm_base_url` | OpenAI-compatible LLM API base URL |
| `llm_api_key` | API key for the LLM provider |
| `llm_model` | LLM model name |
| `interaction_mode` | Front button interaction: `hold_to_talk` or `click_to_talk` |
| `resource_id` | Volcengine resource ID |
| `asr_hotwords` | Comma-separated ASR hotwords; also passed to the LLM as translation terminology hints |
| `paired_device_ids` | Comma-separated 4-digit hex IDs, for example `C3D8,09AF` |
| `device_theme_colors` | Optional per-device overlay colors, for example `C3D8:pink,09AF:green` |
| `device_overlay_positions` | Optional per-device overlay positions, for example `C3D8:top_left,09AF:bottom_right` |
| `auto_enter` | Whether to press Return after paste |
| `debug_audio_cache` | Whether to save debug Ogg Opus files |
| `debug_audio_dir` | Debug audio output directory |
| `[output].target` | `focused_app`, `subtitle`, or `wechat_voice_input`. On Windows, `wechat_voice_input` maps the primary button to the Ctrl+Win WeChat voice-input hotkey instead of starting VoiceStick ASR. |
| `[output].transform` | `original` or `translate` |
| `[output].translation_target` | Target language code for LLM translation, for example `en` or `zh-Hans` |
| `[device.<id>.output]` | Optional per-device override for text transform and translation target |

Supported Volcengine `resource_id` values:

- `volc.seedasr.sauc.duration`
- `volc.seedasr.sauc.concurrent`
- `volc.bigasr.sauc.duration`
- `volc.bigasr.sauc.concurrent`

Do not commit API keys.

## Pairing Flow

1. Flash and boot the StickS3. The screen shows `VS-XXXX`.
2. Start the macOS desktop app.
3. Open `Pair Device...` from the menu bar app.
4. Select the matching `VS-XXXX` in the scan list and click `Pair`.
5. After saving, the desktop app scans for and connects to that device. Repeat this flow to pair additional devices.

You can also edit `paired_device_ids` manually. When multiple IDs are saved, the desktop app ignores nearby unpaired VoiceStick devices.

## Debug Audio

Enable:

```toml
debug_audio_cache = true
```

Default output directory:

```text
~/Library/Application Support/VoiceStick/DebugAudio
```

Each valid recognition session is saved as a playable Ogg Opus file. Recordings shorter than 0.5 seconds are discarded by the desktop app and are not sent to ASR.
