# 9C-HTP100 主机与屏幕联合工程

主机负责执行、治疗状态和计时，屏幕发送请求并显示主机确认的状态。此版本同时修改两端，必须配套升级。

本仓库创建于 2026-11-02，分支 `integration/system-reliability`。两份原仓库保持原状；导入提交保留双方 Git 历史，`baseline/imported` 标签保留合并前基线。当前只创建本地 Git 仓库，未向 GitHub 发布。

| 目录 | 内容 |
| --- | --- |
| `firmware/main` | STM32G070 主机，版本号 20261102 |
| `firmware/screen` | STM32H750 屏幕及 LVGL 资源 |
| `shared/protocol` | 公共帧格式、状态定义、压力计算与配置日志 |
| `tools/pressure_tuner` | Electron + J-Link RTT 临时调参 |
| `tools/firmware_flasher` | Electron + J-Link 主机/新 LCD/旧 LCD 烧录工具 |
| `tests`、`scripts` | 回归测试、联合构建 |
| `out` | 配对固件和 SHA-256 清单，不入 Git |

## 文档

- [问题分析与修改说明](docs/系统整改说明.md)：已修复的问题、保留策略和验证边界。
- [台架验收步骤](docs/台架验收.md)：启动/停止同步、异常断线、传感器、压力和调参测试。
- [主屏通信协议](docs/通信协议.md)：状态、确认、超时和重启处理。
- [调参工具使用](tools/pressure_tuner/README.md)：临时改参及断开恢复。
- [烧录工具使用](tools/firmware_flasher/README.md)：双击根目录 `start_flasher.cmd`；新旧屏幕都含 BOOT。
- [屏幕显示修复及验收](docs/屏幕显示与烧录工具.md)：Setting 标签、启动故障背景及新旧 LCD 验证。

## 构建

需要 Python 3.10+、CMake、Ninja、Arm GNU 工具链。Windows 自动查找 `%LOCALAPPDATA%/stm32cube/bundles/gnu-tools-for-stm32`，也可把工具链加入 PATH。在本目录执行：

```powershell
python scripts/build.py --preset Debug
python scripts/build.py --preset Release
python scripts/build.py --preset Tuning
```

默认构建主机、新 LCD、旧 LCD 三种固件，复制到 `out/<preset>/`。`main.hex` 为主机，`screen-new.hex` 和 `screen-old.hex` 均为 BOOT＋屏幕程序。`Tuning` 使用主机 Tuning + 屏幕 DebugNew/DebugOld；Debug/Release 主机关闭 RTT 调参。`manifest.json` 记录各镜像的 Git 提交、工作区状态、协议版本和 SHA-256。

可加 `--board main|screen-new|screen-old` 单独构建，此时只更新对应的板型 JSON 清单，不刷新全量 `manifest.json`。

打开 `9C-HTP100-System.code-workspace` 可同时查看两端；主机原有 VS Code 调试配置位于 `firmware/main/.vscode`，Debug 配置会先构建再下载。

本次没有烧录硬件。第一次升级时先停止治疗，通过烧录工具分别下载同一输出目录中的主机与对应 LCD 屏幕镜像；两端完成后重新上电。不要把新旧通信版本混用。构建脚本不会连接 J-Link。

## 自动测试

```powershell
python scripts/test.py
node --test tools/pressure_tuner/main.test.cjs
python tools/pressure_tuner/test_bridge.py
python tests/test_ihex.py
node --test tools/firmware_flasher/backend.test.cjs
python scripts/test_screen_ui.py
```

C 测试需要 GCC/Clang；本机也支持 `.cache/native/ziglang/zig.exe cc`。Windows 可用 `python -m pip install --target .cache/native ziglang==0.16.0` 准备编译器。断言始终开启。GitHub Actions 已配置同样的回归，但尚未推送运行。

源码编译和自动测试通过不等于整机验证完成。必须按台架验收表验证真实压力、热响应、屏幕显示、串口时序和 RTT 调参，完成后才能确定这版整机可用。
