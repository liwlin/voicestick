# VoiceStick 0.3.4 M5StickC Plus Release Notes

This release is targeted at **M5Stack M5StickC Plus on Windows**. It packages
the tested M5StickC Plus USB firmware bundle and the Windows portable desktop
app used for formal testing. StickS3 support remains in the source tree, but it
is not the primary packaged target for this release.

## Highlights

- Added M5StickC Plus firmware support as the release focus.
- Added a Windows `WeChat Voice Input` output target.
- Improved Windows BLE connection diagnostics and stale pairing cleanup.
- Added board-specific firmware diagnostics for M5StickC Plus bring-up.
- Added a user manual at `docs/user-manual.md`.

## M5StickC Plus

- Hardware profile: ESP32-PICO-D4, 4 MB flash, AXP192, ST7789v2 display,
  SPM1423 PDM microphone.
- Button A maps to VoiceStick `primary`.
- Button B maps to VoiceStick `secondary`.
- The normal firmware is a USB-flashed factory image. The existing StickS3
  dual-slot OTA layout does not fit M5StickC Plus 4 MB flash.

## Windows WeChat Voice Input

The new Windows output target is:

```toml
[output]
target = "wechat_voice_input"
```

When selected from the tray menu, holding the VoiceStick primary button holds
`Ctrl+Win` on Windows, and releasing the button releases `Ctrl+Win`. This
matches the WeChat desktop voice-input shortcut behavior.

This mode does not start VoiceStick ASR and does not paste text.

## Release Assets

- `VoiceStick-Windows-portable-0.3.4.zip`
  - Windows portable build.
  - Unsigned. Run `VoiceStick.exe` from the extracted folder.
- `voicestick-firmware-m5stickc-plus-0.3.4.zip`
  - M5StickC Plus USB flash bundle.
- StickS3/macOS assets are compatibility outputs when built by the existing
  automation, not the focus of this v0.3.4 package.

## Verification

- Windows unit tests passed.
- Windows application target built successfully.
- M5StickC Plus firmware was previously validated on hardware:
  - display lit
  - buttons verified
  - BLE connection verified
  - audio end-to-end diagnostic verified
  - formal user test succeeded

## Known Notes

- Windows MSI packaging requires local signing certificate and WiX tooling.
  This release includes a portable Windows zip from the local Windows machine.
- M5StickC Plus firmware is not currently offered through the StickS3 OTA
  manifest. Flash it over USB.
