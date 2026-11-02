# J-Link 压力实时调参

macOS 双击 `start.command`，Windows 双击 `start.cmd`。首次启动会使用 npm 安装依赖。连接 J-Link 后选择档位后，可点击“读取当前档位参数”填入设备运行时的四个参数，再修改并发送，无需串口。读取和发送时会短暂停核。

“编译并烧录固件”会编译当前项目的 Debug 固件，通过 J-Link 写入、校验并复位 STM32G070CB。烧录会覆盖设备现有固件；烧录前先停止调试并关闭其他 J-Link 会话。J-Link SWD 速度设置为 1000 kHz。调参使用同一份 `build/Debug/9C-HTP100-main-v2.elf`，固件需要启用 `PRESSURE_JLINK_TUNING_ENABLE`。源码变更后请重新烧录。

macOS 需要安装 Node.js/npm、CMake、Ninja、Arm GNU 工具链（含 `arm-none-eabi-gcc`、`g++`、`objcopy`、`size` 和 `gdb`）及 SEGGER J-Link 软件。工具可放在 PATH 中；启动脚本会加入 `/opt/homebrew/bin` 和 `/usr/local/bin`。也支持 STM32Cube bundles 目录（可设置 `STM32CUBE_BUNDLE_PATH` 或 `CUBE_BUNDLE_PATH`）；如 GDB 或 J-Link GDB Server 不在常见位置，可分别设置 `ARM_GDB` 和 `JLINK_GDB_SERVER` 为可执行文件完整路径。Windows 同样需要上述工具，现有 STM32Cube bundles 可自动识别。

发送时短暂停核，校验成功后继续运行；参数不保存。正常停止调试或关闭窗口时恢复代码表默认值。拔线、强制结束进程无法保证恢复，需重启设备。固件上机使用前请按项目根目录 README 的硬件验证要求进行台架测试。
