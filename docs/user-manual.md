# VoiceStick 用户手册

适用版本：0.3.4

VoiceStick 把 M5Stack StickS3 或 M5StickC Plus 变成一个蓝牙语音输入按钮。设备负责采集音频和发送按键事件，桌面端负责连接设备、识别语音、输出文字，或触发微信电脑端语音输入。

## 1. 准备工作

### 硬件

- M5Stack StickS3，或
- M5Stack M5StickC Plus
- USB 数据线，用于首次刷机和调试
- Windows 10/11 电脑，需支持蓝牙低功耗 BLE

### Windows 软件

Release 中提供 Windows portable 包：

```text
VoiceStick-Windows-portable-0.3.4.zip
```

解压后运行：

```text
VoiceStick.exe
```

程序启动后会进入系统托盘。主要操作都在托盘图标右键菜单里完成。

> 说明：portable 包未安装到系统目录，不会自动创建开始菜单。正式签名 MSI 包需要 Windows 签名环境，后续可单独补充。

## 2. 固件安装

### M5StickC Plus

Release 中提供 M5StickC Plus 固件包：

```text
voicestick-firmware-m5stickc-plus-0.3.4.zip
```

包内包含：

- `bootloader.bin`
- `partition-table.bin`
- `voice_stick.bin`
- `flash-m5stickc-plus.ps1`
- `README.txt`

刷机步骤：

1. 用 USB 数据线连接 M5StickC Plus。
2. 在 Windows 设备管理器中确认串口号，例如 `COM27`。
3. 解压固件包。
4. 在 PowerShell 中进入解压目录。
5. 执行：

```powershell
.\flash-m5stickc-plus.ps1 -Port COM27
```

刷机完成后，设备屏幕应显示 `VS-XXXX`，其中 `XXXX` 是设备 ID。

### StickS3

StickS3 固件由标准 Release workflow 生成 OTA 与 merged 固件。首次安装或分区表变更时，建议通过 USB 刷入 merged 固件；之后可通过桌面端 OTA 升级。

## 3. 首次配对

1. 启动 Windows 端 `VoiceStick.exe`。
2. 右键托盘图标。
3. 选择 `Pair Device...`。
4. 等待列表出现 `VS-XXXX`。
5. 选择对应设备并确认配对。
6. 连接成功后，托盘菜单中会显示该设备状态为 `Connected`。

重要避坑：

- 不要通过 Windows 设置里的“蓝牙和其他设备 > 添加设备”配对 `VS-XXXX`。
- Windows 系统蓝牙配对路径可能留下旧 GATT 记录，导致 App 连接失败或服务发现失败。
- 请始终使用 VoiceStick App 内置的 `Pair Device...` 流程。

## 4. 按钮功能

### M5StickC Plus

- A 键：主按钮，协议角色 `primary`
- B 键：副按钮，协议角色 `secondary`

### StickS3

- 正面按钮：主按钮，协议角色 `primary`
- 侧边按钮：副按钮，协议角色 `secondary`

### Hold to Talk

默认模式。

- 按住主按钮：开始录音或触发当前输出模式
- 松开主按钮：结束录音或释放当前快捷键
- 副按钮：取消当前识别、取消待确认文本，或恢复最近一次可恢复输入

### Click to Talk

托盘菜单：

```text
Interaction -> Click to Talk
```

- 单击主按钮：开始
- 再次单击主按钮：结束
- 副按钮：取消

微信语音输入模式下，如果固件只上报 click 事件，Windows 端会把主按钮 click 当作开关：第一次按下 Ctrl+Win，第二次释放 Ctrl+Win。

## 5. 输出模式

托盘菜单：

```text
Output
```

当前支持三种输出目标。

### Focused App

```text
Output -> Focused App
```

VoiceStick 自己完成语音识别，把最终文本粘贴到当前焦点输入框。

相关选项：

- `Press Return After Paste`：粘贴后是否自动按回车
- Settings 中的 ASR Provider/API Key：控制语音识别服务

适合：

- 普通网页输入框
- 文档编辑
- IDE/聊天窗口
- 需要 VoiceStick Cloud 或 Volcengine ASR 的场景

### Subtitle

```text
Output -> Subtitle
```

VoiceStick 自己完成语音识别，但不粘贴文本，而是在桌面显示字幕。

适合：

- 演示
- 会议字幕
- 不想把内容写入当前应用的场景

每个设备可单独设置字幕颜色和位置。

### WeChat Voice Input

```text
Output -> WeChat Voice Input
```

Windows 端不启动 VoiceStick ASR，而是用主按钮控制微信电脑端的语音输入快捷键。

Hold to Talk 下：

- 按住 A 键：Windows 按住 `Ctrl+Win`
- 松开 A 键：Windows 释放 `Ctrl+Win`

微信电脑端的语音输入要求“按住 Ctrl+Win 期间录音，松开后转文字”。因此这个模式必须保持按键，不是短按一下。

使用步骤：

1. 确认电脑端微信版本支持语音输入。
2. 把光标放在一个可输入文本框中。
3. 托盘菜单选择 `Output -> WeChat Voice Input`。
4. 按住设备 A 键说话。
5. 松开 A 键，等待微信完成转写。

注意：

- 这个模式依赖微信自身语音输入能力。
- VoiceStick 不会调用自己的 ASR，也不会粘贴识别结果。
- 如果微信没有响应，先在键盘上手动按住 `Ctrl+Win` 测试微信功能是否可用。
- 微信快捷键可在微信设置中调整；如果你改了微信快捷键，当前 VoiceStick 版本仍固定发送 `Ctrl+Win`。

## 6. 设置说明

Windows 配置文件路径：

```text
%APPDATA%\VoiceStick\config.toml
```

常见字段：

```toml
asr_provider = "voicestick_cloud"
interaction_mode = "hold_to_talk"
paired_device_ids = "8098"
auto_enter = true
debug_audio_cache = false

[output]
target = "focused_app"
transform = "original"
translation_target = "en"
```

`[output].target` 可选值：

- `focused_app`
- `subtitle`
- `wechat_voice_input`

不要把 API key 提交到 Git 仓库。

## 7. 日常使用建议

- 普通全局语音输入：优先用 `Focused App`。
- 微信语音输入：用 `WeChat Voice Input`。
- 演示或会议：用 `Subtitle`。
- 如果你只想测试蓝牙和按钮，先用 `WeChat Voice Input`，它不依赖 VoiceStick ASR 服务。
- 如果需要排查音频质量，再开启 `debug_audio_cache`，测试后记得关闭并清理缓存。

## 8. 故障排查

### 托盘里看不到设备

1. 确认设备屏幕显示 `VS-XXXX`。
2. 确认 Windows 蓝牙已开启。
3. 不要从 Windows 设置配对，使用 App 的 `Pair Device...`。
4. 如曾从 Windows 设置配对过，删除该蓝牙设备后重启蓝牙再试。

### 已配对但连接失败

1. 退出 VoiceStick。
2. 删除 Windows 设置里的 `VS-XXXX` 残留设备。
3. 重新启动 VoiceStick。
4. 使用 App 内置配对流程重连。

### 微信语音输入没有反应

1. 用键盘手动按住 `Ctrl+Win`，确认微信语音输入本身可用。
2. 确认托盘菜单为 `Output -> WeChat Voice Input`。
3. 确认光标在可输入文本框内。
4. 确认不是短按 A 键，而是按住说话、松开结束。
5. 查看日志：

```text
%LOCALAPPDATA%\VoiceStick\VoiceStickApp.log
```

应能看到：

```text
wechat voice input hotkey down VS-XXXX
wechat voice input hotkey up VS-XXXX
```

### VoiceStick 自己的 ASR 没有结果

1. 检查 Settings 中 ASR Provider 和 API Key。
2. 确认网络可访问对应服务。
3. 关闭 `WeChat Voice Input`，切回 `Focused App`。
4. 如需保留样本，临时开启 debug audio cache。

### 屏幕不亮或状态不更新

M5StickC Plus 依赖 AXP192 电源轨和 LCD 背光配置。请确认刷入的是 M5StickC Plus 固件，而不是 StickS3 固件。

## 9. 升级

Windows portable 包升级：

1. 退出托盘中的 VoiceStick。
2. 解压新版 portable zip。
3. 运行新的 `VoiceStick.exe`。

固件升级：

- StickS3：可使用 OTA 或 USB merged 固件。
- M5StickC Plus：当前使用 USB factory image，按本手册刷机步骤更新。

## 10. 文件清理

可以安全删除的生成物：

- `desktop/windows/build-*`
- `firmware/build*`
- `website/dist`
- `website/node_modules`
- `%LOCALAPPDATA%\VoiceStick\DebugAudio` 下的调试音频

不要删除：

- `%APPDATA%\VoiceStick\config.toml`，除非你想重置 API key 和配对配置。
- Release zip 中的固件 `.bin` 文件，除非确认不再需要刷机。
