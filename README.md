# 9C-HTP100 主机与屏幕联合工程

主机负责执行、治疗状态和计时，屏幕发送请求并显示主机确认的状态。此版本同时修改两端，必须配套升级。

本仓库创建于 2026-11-02，分支 `integration/system-reliability`。两份原仓库保持原状；导入提交保留双方 Git 历史，`baseline/imported` 标签保留合并前基线。当前只创建本地 Git 仓库，未向 GitHub 发布。

| 目录 | 内容 |
| --- | --- |
| `firmware/main` | STM32G070 主机，版本号 20261102 |
| `firmware/screen` | STM32H750 屏幕及 LVGL 资源 |
| `shared/protocol` | 公共帧格式、状态定义、压力计算与配置日志 |
| `tools/pressure_tuner` | Electron + J-Link RTT 临时调参 |
| `tests`、`scripts` | 回归测试、联合构建 |
| `out` | 配对固件和 SHA-256 清单，不入 Git |

## 文档

- [问题分析与修改说明](docs/系统整改说明.md)：已修复的问题、保留策略和验证边界。
- [台架验收步骤](docs/台架验收.md)：启动/停止同步、异常断线、传感器、压力和调参测试。
- [主屏通信协议](docs/通信协议.md)：状态、确认、超时和重启处理。
- [调参工具使用](tools/pressure_tuner/README.md)：临时改参及断开恢复。

## 构建

需要 Python 3、CMake、Ninja、Arm GNU 工具链。Windows 自动查找 `%LOCALAPPDATA%/stm32cube/bundles/gnu-tools-for-stm32`，也可把工具链加入 PATH。在本目录执行：

```powershell
python scripts/build.py --preset Debug
python scripts/build.py --preset Release
python scripts/build.py --preset Tuning
```

每次都构建两端并复制到 `out/<preset>/`。`Tuning` 使用主机 Tuning + 屏幕 Debug；Debug/Release 主机关闭 RTT 调参。`manifest.json` 记录 Git 提交、工作区是否有未提交改动、协议版本及每个镜像的 SHA-256。

打开 `9C-HTP100-System.code-workspace` 可同时查看两端；主机原有 VS Code 调试配置位于 `firmware/main/.vscode`，Debug 配置会先构建再下载。

本次没有烧录硬件。第一次升级时先停止治疗，分别烧录同一输出目录中的主机与屏幕镜像，屏幕沿用原工程的外部 Flash 下载流程；两端完成后重新上电。不要把新旧通信版本混用。构建脚本不会连接 J-Link。

## 自动测试

```powershell
python scripts/test.py
node --test tools/pressure_tuner/main.test.cjs
python tools/pressure_tuner/test_bridge.py
```

C 测试需要 GCC/Clang；本机也支持 `.cache/native/ziglang/zig.exe cc`。Windows 可用 `python -m pip install --target .cache/native ziglang==0.16.0` 准备编译器。断言始终开启。GitHub Actions 已配置同样的回归，但尚未推送运行。

源码编译和自动测试通过不等于整机验证完成。必须按台架验收表验证真实压力、热响应、屏幕显示、串口时序和 RTT 调参，完成后才能确定这版整机可用。
