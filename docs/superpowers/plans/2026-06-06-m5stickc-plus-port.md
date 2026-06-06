# M5StickC Plus Port Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

## Goal

Add M5Stack M5StickC Plus as a second VoiceStick firmware target while preserving
the existing StickS3 firmware behavior and desktop update safety.

## Constraints

- Do not replace the StickS3 target.
- Do not add third-party dependencies.
- Keep BLE service, Opus framing, and desktop protocol unchanged.
- Keep hardware identity strict so firmware update manifests cannot cross-flash
  between boards.
- Build M5StickC Plus as ESP32 with 4 MB flash and no PSRAM.

## Task 1: Create Board Abstraction

- [x] Add `firmware/components/voice_board/include/voice_board.h`.
- [x] Define board identity helpers:
  - `voice_board_hardware_id()`
  - `voice_board_display_name()`
- [x] Define logical button helpers:
  - primary GPIO/pressed state
  - secondary GPIO/pressed state
  - optional power IRQ GPIO
- [x] Define shared power helpers:
  - init
  - battery percent
  - USB present
  - charging
  - power IRQ clear
  - deep sleep preparation
- [x] Define LCD config struct:
  - SPI host
  - resolution
  - x/y gaps
  - MOSI/SCLK/DC/CS/RST/backlight pins
  - backlight mode
  - mirror/swap/invert flags
- [x] Define audio config struct:
  - `VOICE_BOARD_AUDIO_ES8311`
  - `VOICE_BOARD_AUDIO_PDM`
  - I2S port and pins
  - optional codec I2C bus handle

## Task 2: Migrate StickS3 To `voice_board`

- [x] Copy existing StickS3 board logic into
  `firmware/components/voice_board/boards/stick_s3/voice_board_stick_s3.c`.
- [x] Keep existing M5PM1 register initialization and battery calculations.
- [x] Expose existing LCD pins and gaps through `voice_board_lcd_config()`.
- [x] Expose existing ES8311 pins and I2C bus through
  `voice_board_audio_config()`.
- [x] Update `firmware/main/main.c` to include `voice_board.h`.
- [x] Update `firmware/components/ui_status/ui_status.c` to use board LCD and
  backlight config.
- [x] Update `firmware/components/audio_pipeline/audio_pipeline.c` to use board
  audio config for the existing ES8311 path.
- [x] Update `firmware/components/voice_ble/voice_ble.c` so device info uses
  `voice_board_hardware_id()`.

## Task 3: Add M5StickC Plus Board Implementation

- [x] Add
  `firmware/components/voice_board/boards/m5stickc_plus/voice_board_m5stickc_plus.c`.
- [x] Configure I2C on SDA GPIO21 and SCL GPIO22.
- [x] Initialize AXP192 at address `0x34` using the official M5StickC Plus
  rail setup:
  - `0x28 = 0xcc`
  - `0x82 = 0xff`
  - `0x33 = 0xc0`
  - `0x12 |= 0x4d`
  - `0x36 = 0x0c`
  - `0x91 = 0xf0`
  - `0x90 = 0x02`
  - `0x30 = 0x80`
  - `0x39 = 0xfc`
  - `0x35 = 0xa2`
  - `0x32 = 0x46`
- [x] Implement battery voltage and USB presence using AXP192 ADC registers.
- [x] Map primary button to GPIO37 and secondary button to GPIO39.
- [x] Expose LCD pins:
  - MOSI GPIO15
  - SCLK GPIO13
  - DC GPIO23
  - RST GPIO18
  - CS GPIO5
- [x] Expose PDM microphone pins:
  - clock GPIO0
  - data GPIO34
- [x] Disable or limit deep sleep only if the ESP32 wake path is not reliable
  during testing.

## Task 4: Refactor Audio Pipeline For ES8311 And PDM

- [x] Split I2S channel initialization into ES8311 standard RX and PDM RX
  helper functions.
- [x] Keep Opus encoder input format as 16 kHz, 16-bit, mono PCM.
- [x] Include `driver/i2s_pdm.h` only for the PDM path.
- [x] Keep ES8311 codec allocation/deallocation only on StickS3.
- [x] Ensure `audio_pipeline_start()` and `audio_pipeline_stop()` work for both
  board audio kinds.

## Task 5: Add Board Build Configuration

- [x] Add `firmware/components/voice_board/Kconfig` with mutually exclusive
  board choices:
  - `VOICESTICK_BOARD_STICK_S3`
  - `VOICESTICK_BOARD_M5STICKC_PLUS`
- [x] Add `firmware/components/voice_board/CMakeLists.txt` selecting the board
  source from Kconfig.
- [x] Update component dependencies from `stick_s3_board` to `voice_board`.
- [x] Remove the obsolete `firmware/components/stick_s3_board` component so
  ESP-IDF no longer compiles the old board surface.
- [x] Preserve current defaults in `firmware/sdkconfig.defaults.stick_s3`.
- [x] Add `firmware/sdkconfig.defaults.m5stickc_plus`.
- [x] Add `firmware/partitions_m5stickc_plus.csv` for 4 MB factory-style USB
  flashing.
- [x] Keep `firmware/sdkconfig.defaults` usable for the existing StickS3 build.

## Task 6: Documentation

- [x] Update firmware README or top-level docs with M5StickC Plus build command.
- [x] Document expected USB flashing command and supported baud rates.
- [x] Document that first C Plus scope is USB flash, not BLE OTA release.

## Task 7: Verification

- [x] Build StickS3 firmware with the existing default command.
- [x] Build M5StickC Plus firmware with ESP-IDF target `esp32` and the C Plus
  defaults.
- [x] Build M5StickC Plus audio self-test firmware.
- [x] Build website.
- [x] Build and run Windows core tests.
- [x] Detect attached serial ports.
- [x] If an M5StickC Plus serial port is available, flash it and run serial
  monitor checks for:
  - boot
  - LCD init and physical screen/backlight illumination
  - BLE advertising
- [x] Verify M5StickC Plus PDM microphone path with boot self-test:
  10 frames, 9600 samples, non-zero mean/peak PCM statistics.
- [x] Verify BLE advertisement from the Windows host scanner. The C++ WinRT
  probe saw `VS-8098` at `0x94b97eab809a`, with 20 matches in one 25 second
  run.
- [x] Verify physical Button A/Button B events from the device:
  - Button A/GPIO37 produced `button front down` / `button front up`.
  - Button B/GPIO39 produced raw diagnostic transitions plus
    `button side down` / `button side up`.
- [x] Verify audio capture start and Opus delivery to the desktop app with the
  default-off BLE audio e2e diagnostic build:
  - Windows App GATT service discovery succeeded with MTU 247.
  - State and audio notifications subscribed successfully.
  - Firmware sent `seq=0..29`, `dropped=0`, `overflow_drops=0`.
  - Windows App logged `audio notify` START `seq=0 flags=1 payload=150`
    and END `seq=29 flags=2 payload=0`.
- [x] Restore the M5StickC Plus to the normal, non-diagnostic firmware image
  after e2e testing.

## Completion Report

### Development Log

- Created `voice_board` as the board abstraction and moved the original StickS3
  implementation under it. This kept the existing `stick_s3` target intact
  while adding `m5stickc_plus` as a separate hardware identity.
- Added the M5StickC Plus board implementation for ESP32-PICO-D4, 4 MB flash,
  AXP192 PMU, ST7789v2 display, Button A/B, and SPM1423 PDM microphone.
- Reworked the audio pipeline so StickS3 continues using ES8311 standard I2S
  RX and M5StickC Plus uses ESP-IDF PDM RX, both producing the same 16 kHz,
  16-bit, mono Opus-over-BLE stream.
- Reworked UI initialization to read LCD pins, display offsets, inversion, and
  backlight mode from `voice_board`.
- Updated BLE device info to report board hardware as `stick_s3` or
  `m5stickc_plus`, preserving firmware-update safety.
- Added default-off diagnostics:
  - `sdkconfig.defaults.m5stickc_plus.selftest` for boot-time PDM statistics.
  - `sdkconfig.defaults.m5stickc_plus.buttondiag` for raw button transitions.
  - `sdkconfig.defaults.m5stickc_plus.ble_audio_e2e` for automated
    BLE audio delivery validation.
- Improved Windows BLE diagnostics:
  - Clear stale Windows pairing before direct GATT connection.
  - Log audio notify start/end frames.
  - Serialize log writes so BLE/APP/CRD log lines do not interleave.
- Configured a local VoiceStick Cloud trial API key in the Windows app config
  for formal testing. The key is intentionally not committed or documented.

### Verification Evidence

- StickS3 default firmware build passed:
  `idf.py build`, app size `0x14fa80`, 56% free in the smallest app partition.
- M5StickC Plus normal firmware build and USB flash passed:
  app size `0x14a6d0`, 56% free in the 4 MB factory partition layout.
- M5StickC Plus boot log confirmed:
  - ESP32-PICO-D4, 4 MB flash
  - AXP192 init
  - `ui_status: display ready`
  - `voice_ble: advertising as VS-8098`
- The physical screen/backlight was confirmed lit by the user.
- PDM microphone self-test read 10 frames / 9600 samples with non-zero PCM
  statistics.
- Button A/GPIO37 and Button B/GPIO39 were verified from serial logs.
- Windows App connected to `VS-8098` through the app's GATT path:
  - MTU reached 247.
  - service discovery succeeded.
  - state/audio notification subscriptions succeeded.
  - `device_info.hardware` was `m5stickc_plus`.
- BLE audio e2e diagnostic verified:
  - firmware sent `seq=0..29`
  - `dropped=0`
  - `overflow_drops=0`
  - Windows App logged audio START `seq=0 flags=1 payload=150`
  - Windows App logged audio END `seq=29 flags=2 payload=0`
- Formal user test with VoiceStick Cloud trial API succeeded.
- The board was restored to the normal non-diagnostic firmware after testing.
- Windows core tests passed: 1/1.
- Website build passed.
- `git diff --check` passed.

### Pitfalls And Notes

- Do not pair `VS-XXXX` from Windows Settings > Bluetooth > Add device. That
  path can leave stale Windows pairing records and cause WinRT GATT discovery
  failures such as `0x80070016`. Use the VoiceStick app pairing/autoconnect
  path for the custom GATT service.
- Do not leave the BLE audio e2e diagnostic firmware on the device after
  testing. It intentionally simulates a Button A hold at boot once the desktop
  app subscribes.
- Use 115200 baud for reliable flashing on the tested Windows/M5StickC Plus
  setup. Higher baud rates were less reliable locally even though the board
  supports faster rates.
- M5StickC Plus Button A/B are input-only GPIOs with external pull-ups. Internal
  pull-up enable calls are invalid for these pins and should stay disabled.
- The first AXP192 backlight attempt mapped UI brightness too low and the LCD
  stayed dark. C Plus brightness must keep LDO2 in the M5Stack-style usable
  range and explicitly enable the required AXP192 output rails.
- ESP32 original target does not have the same `ESP_EXT1_WAKEUP_ANY_LOW` option
  as newer targets. For one wake pin, `ESP_EXT1_WAKEUP_ALL_LOW` is the correct
  equivalent.
- M5StickC Plus is currently a USB-flashed factory image target. Its 4 MB flash
  does not fit the existing StickS3 dual 3 MB OTA-slot layout.
- The desktop firmware manifest currently identifies the release firmware as
  `stick_s3`, so update prompts must remain hardware-gated and should not offer
  StickS3 firmware to C Plus.
- API keys and generated app config are local machine state. Never commit them.

## Follow-Up: Windows WeChat Voice Input Output Target

### Implementation

- Added a Windows output target named `wechat_voice_input`.
- Exposed it from the tray `Output` menu as `WeChat Voice Input`.
- In this target, the Windows coordinator maps the VoiceStick primary button to
  a local `Ctrl+Win` hotkey injection and does not start VoiceStick ASR.
- Firmware behavior is unchanged. The device still reports normal
  `button_down`, `button_up`, or `button_click` events; the Windows app decides
  whether those events start ASR or trigger the WeChat voice-input hotkey.
- Added tests that verify:
  - `wechat_voice_input` parses/formats as an output target.
  - the primary button triggers one WeChat hotkey.
  - ASR does not start.
  - captured audio frames are ignored.
  - no text paste occurs.

### Verification Evidence

- Red test first: Windows tests failed because `kWechatVoiceInput` and
  `PressWechatVoiceInputHotkey()` did not exist.
- Green verification:
  - `cmake --build desktop\windows\build-codex --config Debug --target voicestick_windows_tests`
  - `ctest --test-dir desktop\windows\build-codex -C Debug --output-on-failure`
  - result: 1/1 Windows tests passed.
  - `cmake --build desktop\windows\build-codex --config Debug --target VoiceStickApp`
  - result: Windows app target linked successfully.

### Pitfalls And Notes

- `wechat_voice_input` is intentionally a Windows-side output target, not a
  firmware interaction mode. Do not add a BLE protocol field for it unless a
  future host platform also needs to coordinate this mode explicitly.
- In hold-to-talk firmware mode, the device starts recording on primary button
  down and stops on button up. The first host `ready` state can be ignored by
  firmware while recording, so Windows sends `ready` again on button up without
  firing the hotkey a second time.
- The hotkey injection sends the modifier pair `Ctrl+Win` only. It does not
  paste text, press Return, or use the ASR result path.
