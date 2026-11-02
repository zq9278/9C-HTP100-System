# HTP100 固件烧录工具

## 分发版（接收方使用）

`out/distribution/` 中的 ZIP 包含独立 Windows x64 应用、J-Link 程序及 USB 驱动、主机/新 LCD/旧 LCD 三种 Release 固件。完整解压后双击 `HTP100-Flasher.exe`，不需要安装 Node.js、Python 或编译器，也不依赖原工程目录。

分发版只烧录内置固件，选择板型后点击“开始烧录”。首次没有驱动时点击“安装 J-Link 驱动”，按 Windows 提示完成安装。窗口固定为 1040×780 内容区域，禁止调整大小/最大化；屏幕可用空间不足时保持比例整体缩小。

成功后显示绿色“烧录成功”弹窗（含板型、时间）并保留绿色状态；失败显示错误和日志，不弹成功提示。固件和 J-Link 运行文件在烧录前校验 SHA-256，文件损坏会阻止下载。

在开发电脑重新生成分发包：

```powershell
python scripts/package_flasher.py
```

默认先重新构建三种 Release 固件；`--skip-build` 仅打包已验证的现有 Release 文件。可以用 `--jlink "C:/Program Files/SEGGER/JLink_V954"` 指定运行组件目录。打包过程不连接硬件。ZIP 旁生成 `.sha256` 文件。

J-Link 原许可随包保留；对外分发条件见 [分发说明](DISTRIBUTION.md) 与随包 `SEGGER-LICENSE.txt`。本工具未替代第三方分发授权。

## 开发版（本机使用）

在联合仓库双击 `start_flasher.cmd`，或本目录 `start.cmd`。首次启动会安装 Electron 依赖。需要 Node.js/npm、Python 3.10+、CMake、Ninja、Arm GNU 工具链和 SEGGER J-Link；当前电脑已安装并完成软件测试。

## 使用

1. 停止治疗，连接需要升级的控制板，关闭 VS Code/J-Flash 等占用探针的程序。
2. 选择“主机控制板”“新屏幕”或“旧屏幕”。新旧按 LCD 实物型号选择，工具不能自动识别面板型号。
3. 通常选择 `Release`。保留“烧录前重新编译最新代码”，避免下载过期文件。
4. 工具自动查找 `JLink.exe`；找不到时手动选择。仅连接一个探针时序列号可留空，多探针时填写相应序列号。
5. 点击“编译并烧录”，等待完成。也可以点击“仅编译并生成固件”，这不会连接硬件。
6. 主机和对应 LCD 屏幕分别更新完成后，重新上电验证。两端需使用本联合工程配套协议。

启动应用、选择板型、查看固件信息均不会连接或下载硬件。点击烧录后才执行下载。运行中禁止重复任务和关闭窗口；错误会显示执行日志。

## 三种输出

| 选择 | `out/Release` 文件 | 内容 |
| --- | --- | --- |
| 主机 | `main.hex` | STM32G070CB 主机程序 |
| 新屏幕 | `screen-new.hex` | BOOT＋`HTP_LCD_NEW=1` 的屏幕程序 |
| 旧屏幕 | `screen-old.hex` | 同一个 BOOT＋`HTP_LCD_NEW=0` 的屏幕程序 |

**新屏幕和旧屏幕都烧录 BOOT＋屏幕程序。** `screen-*-app.hex/bin/elf` 是单独的屏幕应用产物，供调试使用。上位机只下载上述完整 HEX。不要再使用历史输出 `screen.hex`。

屏幕 BOOT 位于 `0x08000000`，应用位于外部 Flash `0x90000000`；保留稀疏地址，不生成跨地址空洞的巨大 BIN。合并时拒绝重叠、越界、校验错误和无效向量表。

新旧 LCD 沿用 `firmware/screen/HardWare/Src/Lcd_int.c` 中已有的两套初始化序列，寄存器参数没有重新设计。ReleaseNew/ReleaseOld 和 DebugNew/DebugOld 独立构建目录，避免宏切换后复用错误对象文件。Tuning 对主机启用调参，对屏幕使用对应 Debug 变体。

每种板型都有同名 JSON 清单，记录 SHA-256、Git 提交、是否含未提交修改、构建时间和 BOOT 哈希。全量构建另生成 `manifest.json`；单板构建只更新该板型 JSON，上位机以该文件为准。

```powershell
python scripts/build.py --preset Release
python scripts/build.py --preset Release --board screen-old
python scripts/package_info.py --role screen-old --preset Release
```

## 下载实现与验证范围

主机使用 `STM32G070CB`，屏幕使用 `STM32H750XB`。屏幕的 QSPI 下载器沿用提供的 `screen.jflash` 引脚配置：`CLK@PF10_nCS@PB6_D0@PF8_D1@PF9_D2@PF7_D3@PF6`。若实物旧屏控制板的 QSPI 接线不同，需要另配下载器；LCD 新旧选项只切换面板初始化。

下载前核对 HEX、板型和 SHA-256，并冻结本次文件副本；编译失败不会继续下载旧文件。使用 J-Link Commander `LoadFile` 的内置下载校验，成功后请求复位运行。未加入整片擦除、解锁或 Option Bytes 修改命令；写入范围对应扇区仍由下载器执行必要擦除。探针固件自动更新已关闭。

命令依据：[SEGGER J-Link Commander](https://kb.segger.com/J-Link_Commander)、[J-Link Command Strings](https://kb.segger.com/J-Link_Command_Strings)。

本机已验证三个目标构建、HEX 合并/损坏拒绝、并发拒绝、失败中止和 Electron 界面启动。**尚未进行真实 J-Link 下载及新旧面板点亮验证。** 首次分别烧录三个板型并保留日志；两种屏幕均需断电重启确认 BOOT 能正常启动外部程序。
