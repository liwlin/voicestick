# M5StickC Plus Firmware Port Design

## Purpose

Port the VoiceStick firmware from the current M5Stack StickS3 board support to
M5Stack StickC-Plus while preserving the existing StickS3 target.

The target board is M5Stack StickC-Plus, SKU K016-P. The official M5Stack
documentation identifies it as an ESP32-PICO-D4 based device with 4 MB flash,
ST7789v2 135 x 240 LCD, SPM1423 microphone, AXP192 PMU, GPIO37/GPIO39 buttons,
and ESP-IDF support:

https://docs.m5stack.com/en/core/m5stickc_plus

## Selected Approach

Add M5StickC Plus as a second firmware target rather than replacing StickS3.

The existing repository already has desktop pairing, firmware update, website,
and release logic that assumes `stick_s3` hardware. Replacing that target would
risk pushing incompatible firmware images to existing devices. A second target
lets the same BLE protocol and desktop app support both boards while keeping
hardware identity explicit.

Rejected alternatives:

- Replace StickS3 with M5StickC Plus: simpler locally, but breaks existing
  StickS3 builds, release naming, OTA compatibility, and documentation.
- Add board support through scattered `#ifdef` blocks in current files: faster
  initially, but spreads board-specific GPIO, PMU, display, and audio logic
  across application code.

## Hardware Mapping

M5StickC Plus hardware facts from official documentation:

| Capability | M5StickC Plus |
| --- | --- |
| SoC | ESP32-PICO-D4 |
| Flash | 4 MB |
| Display | ST7789v2, 135 x 240 |
| LCD pins | MOSI GPIO15, CLK GPIO13, DC GPIO23, RST GPIO18, CS GPIO5 |
| Microphone | SPM1423 |
| Mic pins | CLK GPIO0, DATA GPIO34 |
| PMU | AXP192 |
| I2C pins | SCL GPIO22, SDA GPIO21 |
| Buttons | Button A GPIO37, Button B GPIO39 |
| Buzzer | GPIO2 |
| Red LED | GPIO10 |

Existing StickS3 facts from repository code:

| Capability | StickS3 |
| --- | --- |
| SoC target | ESP32-S3 |
| Flash | 8 MB |
| Display | ST7789 variant, 135 x 240 |
| Audio | ES8311 codec over I2S |
| PMU | M5PM1 |
| Original board component | `firmware/components/stick_s3_board` before migration |

## Architecture

Introduce a board abstraction component named `voice_board`.

`voice_board` exposes the hardware capabilities used by app, UI, BLE, and audio
code:

- board identity: hardware string, display name, target capabilities
- button GPIOs and active level
- I2C bus setup for PMU and optional codec control
- battery, USB power, charge status, and PMU IRQ helpers
- LCD pin and panel configuration
- audio input type and pins
- sleep preparation and wake GPIO

Board-specific implementations live behind the same public header:

- `boards/stick_s3/voice_board_stick_s3.c`
- `boards/m5stickc_plus/voice_board_m5stickc_plus.c`

The existing `stick_s3_board` API should be removed from application-facing
code after migration. Application code should include `voice_board.h`.

## Build Configuration

Use explicit sdkconfig defaults per board:

- `firmware/sdkconfig.defaults.stick_s3`
- `firmware/sdkconfig.defaults.m5stickc_plus`

StickS3 keeps:

- `CONFIG_IDF_TARGET="esp32s3"`
- 8 MB flash
- PSRAM enabled
- existing OTA partition table

M5StickC Plus uses:

- `CONFIG_IDF_TARGET="esp32"`
- 4 MB flash
- no PSRAM
- a new 4 MB partition table

The 4 MB target cannot reuse the current two 3 MB OTA slots. First-stage C Plus
support should use a factory-style USB-flashable image unless the compiled app
size leaves room for a safe OTA table. BLE OTA for C Plus can be added later
with a board-specific partition layout if size allows.

## Audio Design

Keep Opus framing and BLE audio protocol unchanged.

Refactor `audio_pipeline.c` so it asks `voice_board` which input path to use:

- StickS3 path: current ES8311 codec setup and standard I2S RX.
- M5StickC Plus path: ESP-IDF PDM RX using `driver/i2s_pdm.h`.

The M5StickC Plus SPM1423 path should produce 16 kHz, 16-bit, mono PCM before
Opus encoding, matching the current BLE and desktop ASR expectations.

## Display Design

Keep the current LVGL UI and ST7789 rendering model.

Move LCD constants and GPIOs into board-provided config:

- resolution
- x/y panel gap
- SPI host
- MOSI/SCK/DC/CS/RST GPIOs
- backlight control strategy
- color inversion and mirror settings

StickS3 continues using GPIO PWM backlight. M5StickC Plus display power and
backlight are controlled through AXP192 rails; if direct PWM is unavailable, the
board implementation should expose brightness as coarse PMU on/off or level
control rather than faking PWM.

## Power And Buttons

Map logical VoiceStick buttons to C Plus buttons:

- `primary` -> Button A, GPIO37
- `secondary` -> Button B, GPIO39

Keep the desktop interaction model unchanged. Firmware sends raw button facts
only: `button_down`, `button_up`, and `button_click`.

For deep sleep:

- StickS3 keeps current front-button wake behavior.
- C Plus should use Button A wake if ESP32 sleep wake capabilities support it
  on GPIO37. If deep sleep wake cannot be reliable on that RTC-capable input
  in the first implementation, C Plus should still dim/idle correctly and log
  that deep sleep is disabled for the board.

## BLE And Desktop Compatibility

The BLE service, characteristics, audio frame, control frame, and state frame
formats remain unchanged.

`voice_ble_send_device_info()` must use the board identity:

- StickS3: `hardware:"stick_s3"`
- M5StickC Plus: `hardware:"m5stickc_plus"`

Desktop firmware update compatibility currently compares device hardware to the
manifest hardware. That behavior must remain strict so a StickS3 OTA image is
not offered to a C Plus device.

## Website And Release Scope

First implementation scope:

- local C Plus firmware build
- USB flash instructions
- safe hardware identity separation

Out of scope for first implementation:

- publishing C Plus OTA images from GitHub Actions
- multi-hardware appcast/firmware manifest selection
- automatic website flasher target selector

Those release features should be planned after the C Plus firmware boots,
records audio, and pairs with the desktop app.

## Testing Strategy

Required static/build verification:

- Build existing StickS3 firmware with ESP-IDF.
- Build new M5StickC Plus firmware with ESP-IDF.
- Build website to ensure documentation/link changes do not break Vite.
- Build and run Windows core tests because firmware hardware strings and
  manifest compatibility affect desktop update behavior.

Required device verification for M5StickC Plus:

- Flash the M5StickC Plus over USB.
- Confirm screen boots and shows the VoiceStick device name.
- Confirm BLE advertises as `VS-XXXX`.
- Pair from the desktop app.
- Press Button A and confirm desktop sees primary press/release.
- Press Button B and confirm desktop sees secondary click.
- Record at least one utterance and confirm Opus-over-BLE reaches ASR.
- Confirm `device_info.hardware` is `m5stickc_plus`.

## Risks

- ESP32 4 MB flash may not fit a dual-OTA layout with the current BLE, LVGL,
  Opus, and UI assets.
- ESP32 PDM RX behavior may need gain or slot-mask tuning on real hardware.
- AXP192 register handling is different from current M5PM1 code and may require
  adjustment after device testing.
- Display gap/inversion may need one hardware iteration to match orientation.
- The local Windows machine may need FTDI drivers before M5StickC Plus exposes a
  usable serial port.

## Acceptance Criteria

The port is complete when:

- StickS3 still builds successfully.
- M5StickC Plus builds as an `esp32` target with 4 MB flash settings.
- M5StickC Plus firmware boots on the device.
- M5StickC Plus displays VoiceStick UI.
- M5StickC Plus pairs with the existing desktop app over the same BLE protocol.
- Button A and Button B map to `primary` and `secondary`.
- SPM1423 audio reaches the desktop app as Opus BLE frames and can be sent to
  ASR.
- Desktop update logic does not offer StickS3 firmware to M5StickC Plus.
