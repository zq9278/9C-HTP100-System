# 屏幕 BOOT

`BOOT.hex` 原样复制自用户指定文件：

`D:\Project\9C-HTP100\software\固件\20260702\screen\BOOT.hex`

- SHA-256：`d651640b3fb17cc3478297f28b7c92c47abb15ea47e3a3b2bbd2607862fb7944`
- 内部 Flash 数据起点：`0x08000000`，实际数据 16720 字节。
- 新 LCD 和旧 LCD 固件都合入此 BOOT；区别仅在屏幕程序的 LCD 初始化选项。
- BOOT 源码不在此次修改范围内，本文件未改动。构建时校验地址、向量表和 HEX 校验和，烧录前再次核对合并后的 BOOT 字节。
