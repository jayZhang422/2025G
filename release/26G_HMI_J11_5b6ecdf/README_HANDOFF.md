# 26G J11 串口屏冻结移交包

本目录用于上板验证和移交串口屏负责人所负责的部分：J11 外引脚、PL AXI
UARTLite、PS 双向交互代码、最终 TJC HMI 工程及匹配的离线构建制品。

固定结论：

- 最终接口是 J11，J12/UART0 已放弃。
- 串口是 115200 8N1。
- 本工程不需要 DAC，F16/F17 已用于串口屏。
- 不包含也不生成 `BOOT.BIN`。
- 逻辑源码基线是 `5b6ecdf27767cbd1aafda96ecba92069a024c922`。

## 1. 先核对文件

在本目录打开 PowerShell：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\verify_release.ps1
```

必须看到全部文件哈希通过且未发现 `BOOT.BIN`。若任一文件失败，不要混用同名
bit、XSA、ELF 或 `ps7_init.tcl`。

## 2. 接线

断电后连接：

| J11 | FPGA | 方向 | 屏幕侧 |
| --- | --- | --- | --- |
| PIN3 | F17 | FPGA TX | 屏幕/模块 RX |
| PIN4 | F16 | FPGA RX | 经过降压保护的屏幕/模块 TX |
| PIN1 | GND | 信号地 | 屏幕/模块 GND |

不要用 J11 PIN2 给屏幕供电。屏幕适配模块 USB 保持断开；接入 F16 前，先实测
降压后的 TX 电平对 3.3 V FPGA 输入安全。

## 3. 烧录屏幕

用 TJC/USART HMI 工具打开 `hmi/26G.HMI`，确认 115200 8N1，编译并下载到
屏幕。烧录完成后关闭工具并拔掉适配模块 USB。HMI 工程需要单独烧录，不在 FPGA
bit、XSA 或 ELF 中。

## 4. JTAG 运行最终候选

连接 AX7020 JTAG 和 PS UART1，PS UART1 使用 115200 8N1、无流控。确认接线和
输入电平后执行：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\run_j11_candidate.ps1
```

若 Vitis 2020.2 不在默认安装位置：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File .\run_j11_candidate.ps1 `
  -XsctPath 'D:\your_path\Vitis\2020.2\bin\xsct.bat'
```

正常启动日志应包含：

- `[HMI] READY: J11 UARTLite 115200 8N1`
- 触摸后的 `[HMI] EVENT page=... command=...`
- 首次页面发送完成后的 `[HMI] DISPLAY_TX_COMPLETE ... elapsed=... ms`

`DISPLAY_TX_COMPLETE` 只证明 FPGA 已发完最后一个串口字节。题目要求的 2 秒需
从触摸 START 到屏幕画面实际稳定，通过录像或现场计时确认。

## 5. 文件用途

- `artifacts/`：严格匹配的 bit、XSA、ELF 和 `ps7_init.tcl`。
- `hmi/26G.HMI`：最终串口屏工程。
- `source_tree/25G_PL/ip_core/pl_hmi_uart/`：按队友工程结构整理的可移植 PL 串口
  IP 源码、usage 说明和 `README.md`；实际核心是官方 `axi_uartlite:2.0`，没有伪造
  的 `component.xml`。
- `source_tree/`：串口 IP 集成、顶层/XDC、PS HMI 代码、测试和构建脚本的路径化
  副本；完整工程以 GitHub `codex/hmi-final-j11` 分支为准。
- `reports/`：最终实现的时序、DRC、CDC、I/O、bus skew、资源报告。
- `logs/`：独立 BD、PL 集成、PL 完整构建、Vitis 构建和 host 回归日志。
- `docs/`：详细集成说明与可填写的上板验收表。
- `SHA256SUMS.txt`：除自身以外所有移交文件的 SHA-256 清单。

## 6. 证据边界

最终离线构建已通过，时序为 WNS `+0.752 ns`、WHS `+0.035 ns`，DRC 为 0
Error。继承设计仍有非 UART CDC Critical，不能宣称全系统 CDC clean。

较早的 J11 候选曾完成一次 F17 到 F16 的 1024-byte 物理环回，但本目录最终
bit/XSA/ELF 与实际 TJC 的双向触摸、绘图和 2 秒端到端显示仍必须按
`docs/26G_HMI_J11_BOARD_TEST.md` 留证。

本次交付前没有连接 AX7020、JTAG、串口屏或 CP210x，因此尚未开始板级测试；
这不是工程失败。接线、供电和电平检查完成后，再执行第 4 节的下载步骤。

队长最新算法主线在本包冻结后另有改动。若把这些 HMI 文件移植到更新主线，必须
重新构建 ELF；只要 RTL、XDC、BD 或 IP 参数变化，还必须重新构建 bit/XSA。
