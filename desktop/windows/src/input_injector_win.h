#pragma once

#include "voice_stick_coordinator.h"

#include <Windows.h>

#include <string>

namespace voicestick {

class InputInjectorWin : public InputInjector {
public:
    void Paste(const std::string& text, bool press_enter) override;
    void SetWechatVoiceInputHotkeyDown(bool is_down) override;

private:
    static std::wstring Utf16FromUtf8(const std::string& text);
    static void SendKey(WORD virtual_key, bool key_down, DWORD flags = 0);
    static void SendCtrlV();
    static void SendEnter();
    static void SendCtrlWinDown();
    static void SendCtrlWinUp();
};

} // namespace voicestick
