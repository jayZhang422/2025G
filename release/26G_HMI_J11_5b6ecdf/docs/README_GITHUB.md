# 2026 G 题串口屏最终交付（J11 / AXI UARTLite）

本分支只说明并交付串口屏与 FPGA 的最终交互链路。最终方案固定为：

`TJC 7 英寸串口屏 <-> AX7020 J11 <-> PL AXI UARTLite <-> PS FreeRTOS HMI 任务`

- 最终接口：J11，不使用 J12/UART0。
- 串口格式：115200 baud、8 数据位、无校验、1 停止位（115200 8N1）。
- FPGA：ALINX AX7020，`xc7z020clg400-2`。
- 工具：Vivado 2020.2、Vitis 2020.2。
- 本工程不需要 DAC。顶层已经撤销外部 DAC 端口；`H_top` 内部旧接口名仅为兼容保留且悬空。
- 本交付不生成 `BOOT.BIN`，也不使用 Bootgen。

## 1. 先理解整条链路

```text
屏幕触摸事件
  -> 屏幕 TX
  -> 降压/电平保护
  -> J11 PIN4 / FPGA F16 / i_hmi_uart_rx
  -> AXI UARTLite RX FIFO
  -> pl_hmi_uart.c 轮询驱动和 hmi_protocol.c 帧解析
  -> g26_hmi_task.c
  -> FreeRTOS 测量请求队列
  -> g26_measurement_app.c 执行采集与分析
  -> 结果快照
  -> g26_hmi_task.c 生成 TJC 命令
  -> AXI UARTLite TX FIFO
  -> J11 PIN3 / FPGA F17 / o_hmi_uart_tx
  -> 屏幕 RX
  -> 波形、频谱和参数显示
```

AXI UARTLite 是 PL 内的 Xilinx IP。PS 通过 AXI4-Lite 访问它；当前 XSA 中地址为
`0x43C30000..0x43C3FFFF`，软件只使用当前 BSP 的
`XPAR_UARTLITE_0_*` 宏，不硬编码地址。UARTLite 的波特率在 Vivado 构建时固定，
不能只改 C 程序就改变波特率。

PS UART1 仍是独立调试串口，保持 115200 8N1，用于查看 `[HMI]` 和 `[G26]`
日志。它不是屏幕数据口。

## 2. 最终接线

所有接线都在断电状态下完成。

| AX7020 J11 | FPGA 管脚 | 方向（相对 FPGA） | 接到屏幕侧 | 说明 |
| --- | --- | --- | --- | --- |
| PIN3 | F17 | 输出 | 屏幕/模块 RX | `o_hmi_uart_tx`，LVCMOS33 |
| PIN4 | F16 | 输入 | 经降压保护后的屏幕/模块 TX | `i_hmi_uart_rx`，LVCMOS33 |
| PIN1 | GND | - | 屏幕/模块 GND | 必须共地 |

不要使用 J11 PIN2 给 7 英寸屏供电。屏幕从整机同一路 5 V 配电取电，降压模块
和配电实现由硬件负责人处理。本交付只要求在接入 F16 前，实测保护后 TX 高电平
对 3.3 V FPGA 输入有安全余量。

屏幕适配模块的 USB 在正常 FPGA 联调时保持断开，除非已经证明 USB-UART 与
排针 TX/RX 隔离。屏幕曾出现 3.56 V TX 高电平，因此禁止绕过降压/保护直接接 F16。

## 3. GitHub 上需要取得什么

仓库：`jayZhang422/2025G`

最终分支：`codex/hmi-final-j11`

```powershell
git fetch origin codex/hmi-final-j11
git switch --create codex/hmi-final-j11 --track origin/codex/hmi-final-j11
```

若本地已经存在该分支，使用 `git switch codex/hmi-final-j11`，不要从旧的
`calvin-uart-integration` 或 J12 实验文件继续集成。

制品对应的逻辑源码基线是提交：

`5b6ecdf27767cbd1aafda96ecba92069a024c922`

此后的提交只允许补充交付文档、manifest 和板测证据；若 RTL、XDC、BD 参数或
PS 功能源码发生变化，原 bit/XSA/ELF 就不再与源码对应，必须重新构建并更新哈希。

本分支内可直接下载的冻结移交目录为：

`release/26G_HMI_J11_5b6ecdf/`

进入该目录后先运行：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\verify_release.ps1
```

再按其中的 `README_移交.md` 上板。

### 与队长最新算法主线的边界

本移交打包时，远端 `origin/main` 已到 `39c3066`，它不是本冻结候选的祖先，且
包含算法标定和默认换算参数变化。串口屏负责人没有把这项队长算法改动并入本分支，
也没有据此替换已经完成对应性审计的 bit/XSA/ELF。

- 只验证串口屏负责人部分：直接使用冻结 release 中五个匹配制品。
- 合入队长最新算法：以队长主线为基线移植本分支的 UART/HMI 文件，解决
  `g26_measurement_app.c` 的共同改动后，重新构建并重新板测；此时不得继续使用本
  release 的 ELF，也不得沿用旧哈希宣称制品对应。

这条边界避免把“J11/HMI 最终版本”误解成“全队算法主线的最新版本”。

## 4. 新文件分别做什么

| 路径 | 用途 | 是否需要交给队长 |
| --- | --- | --- |
| `25G_PL/script/pl_hmi_uart_bd.tcl` | 创建可移植 AXI UARTLite 层级，固定 115200 8N1/100 MHz | 是，串口 IP 的可复现定义 |
| `25G_PL/script/pl_integrate_hmi_uart.tcl` | 将 UARTLite 接到现有 BD 的 `M05_AXI`，检查原 M00..M04 和 SD0 后再集成 | 是 |
| `25G_PL/script/pl_validate_hmi_uart_bd.tcl` | 不依赖完整工程的独立 BD 结构检查 | 是 |
| `25G_PL/script/pl_build_hmi_uart_candidate.tcl` | 完整综合、实现、报告、bit/XSA 导出 | 是 |
| `25G_PL/25G_PL.srcs/sources_1/new/top.v` | 暴露 `i_hmi_uart_rx`/`o_hmi_uart_tx`，撤销外部 DAC 端口 | 是 |
| `25G_PL/25G_PL.srcs/constrs_1/new/da_hw_275.xdc` | F16/F17 的 J11 LVCMOS33 约束；旧文件名为兼容保留 | 是 |
| `25G_PS/.../pl_hmi_uart.[ch]` | UARTLite 轮询驱动、FIFO 分段发送、超时和页面查询 | 是 |
| `25G_PS/.../hmi_protocol.[ch]` | 触摸帧和 `sendme` 回复解析 | 是 |
| `25G_PS/.../g26_hmi_task.[ch]` | 唯一 UART 所有者；处理触摸、页面更新和并发会话 | 是 |
| `25G_PS/.../g26_hmi_render.[ch]` | 波形重采样、数值格式化和频谱坐标映射 | 是 |
| `25G_PS/.../g26_measurement_app.[ch]` | HMI 请求队列、结果互斥快照和完成队列 | 是 |
| `25G_PS/tests/g26_hmi_host_test.c` | 协议、会话、坐标和 640->601 点回归 | 是 |
| `25G_PS/script/ps_build_hmi_candidate.tcl` | 从指定 XSA 创建隔离 Vitis 平台并构建应用 | 是 |
| `25G_PS/script/ps_run_hmi_candidate.tcl` | 使用匹配 bit/XSA/ps7_init/ELF 的无 FSBL JTAG 下载 | 是 |
| `HMI/26G.HMI` | 最终 TJC 页面、控件、图片和触摸脚本工程 | 是，需单独烧入屏幕 |
| `Doc/26G_HMI_J11_INTEGRATION.md` | 完整软件/硬件合同与验证边界 | 是 |
| `Doc/26G_HMI_J11_BOARD_TEST.md` | 上板步骤和结果记录表 | 是 |

表中的 `25G_PS/...` 是
`25G_PS/Identification_Processing_System/src/User/` 下的 `include` 或 `src` 文件。

## 5. HMI 页面和协议

屏幕发给 FPGA 的触摸帧固定为 5 字节：

`AA 55 PAGE COMMAND FF`

| 页面 | 操作 | PAGE/COMMAND |
| --- | --- | --- |
| page2 | START | `02 01` |
| page2 | STOP | `02 00` |
| page2 | 1 周期 | `02 11` |
| page2 | 显示时域参数 | `02 12` |
| page2 | 3 周期 | `02 33` |
| page3 | START | `03 01` |
| page3 | STOP | `03 00` |
| page3 | 显示各谱线幅值 | `03 11` |

FPGA 发给屏幕的是 ASCII TJC 指令，每条指令末尾追加 `FF FF FF`。

- page2：波形控件 ID 1、通道 0；640 点视图重采样为 601 点发送。
- page2：`x0` 为 Upp（mV），`x1` 为真 RMS（mV），`n0` 为基频（kHz）。
- page3：`x0..x2` 为频率，`t0..t2` 为正弦峰值幅度，`t7` 为第三行标签。
- `status` 和 `status1` 由 HMI 脚本独占，PS 不写这两个对象。

不要自行改页面号、控件名、波形 ID 或背景图尺寸。任何修改都要同步 C 头文件、
HMI 工程和 host 回归；背景图尺寸变化还必须重新标定频谱坐标。

## 6. 如何烧录 HMI

1. 使用当前 TJC/USART HMI 工具打开 `HMI/26G.HMI`。
2. 确认项目串口为 115200 8N1，并编译/下载到屏幕。
3. 烧录完成后关闭工具，拔掉适配模块 USB。
4. 再按第 2 节连接 J11 与屏幕。

HMI 工程不包含在 bit、XSA、ELF 或 `BOOT.BIN` 中，必须单独烧入屏幕。

## 7. 从干净分支重建

在一个可丢弃的干净副本中运行，输出目录必须放在源码工程外。先生成当前 BD
wrapper，再构建 PL：

```powershell
& 'D:\Vivado_install\Vivado\2020.2\bin\vivado.bat' `
  -mode batch -nojournal -nolog `
  -source 25G_PL/script/pl_integrate_hmi_uart.tcl

$env:PL_HMI_UART_CANDIDATE_DIR = 'D:\path\to\new_pl_candidate'
& 'D:\Vivado_install\Vivado\2020.2\bin\vivado.bat' `
  -mode batch -nojournal -nolog `
  -source 25G_PL/script/pl_build_hmi_uart_candidate.tcl
```

再用刚导出的 XSA 建立隔离 Vitis 工作区：

```powershell
$env:PS_HMI_CANDIDATE_XSA = `
  'D:\path\to\new_pl_candidate\25g_2026g_hmi_j11.xsa'
$env:PS_HMI_CANDIDATE_WORKSPACE = 'D:\path\to\new_vitis_workspace'
& 'D:\Vivado_install\Vitis\2020.2\bin\xsct.bat' `
  25G_PS/script/ps_build_hmi_candidate.tcl
```

不要使用仓库中的历史 `25G_PL/top.xsa`，也不要只凭同名文件判断版本。

## 8. 用最终制品上板

队长移交包内应同时包含下列四个匹配文件：

- `25g_2026g_hmi_j11.bit`
- `25g_2026g_hmi_j11.xsa`
- `ps7_init.tcl`
- `hmi_candidate_app.elf`

在板卡 JTAG 和 PS UART1 已连接后，从移交包根目录运行：

```powershell
$env:PS_HMI_CANDIDATE_BIT = `
  (Resolve-Path '.\artifacts\25g_2026g_hmi_j11.bit')
$env:PS_HMI_CANDIDATE_XSA = `
  (Resolve-Path '.\artifacts\25g_2026g_hmi_j11.xsa')
$env:PS_HMI_CANDIDATE_PS7_INIT = `
  (Resolve-Path '.\artifacts\ps7_init.tcl')
$env:PS_HMI_CANDIDATE_ELF = `
  (Resolve-Path '.\artifacts\hmi_candidate_app.elf')
& 'D:\Vivado_install\Vitis\2020.2\bin\xsct.bat' `
  '.\source_tree\25G_PS\script\ps_run_hmi_candidate.tcl'
```

PS UART1 使用 115200 8N1、无流控。正常启动应看到：

- `[HMI] READY: J11 UARTLite 115200 8N1`
- 触摸后出现 `[HMI] EVENT page=... command=...`
- 首屏发送完出现 `[HMI] DISPLAY_TX_COMPLETE ... elapsed=... ms`

`DISPLAY_TX_COMPLETE` 只表示最后一个 UART 字节已经离开 FPGA，不等于屏幕已经完成
物理渲染。题目要求的 2 秒必须从触摸 START 到人眼/录像确认最终画面稳定为止。

## 9. 当前最终制品

逻辑源码提交：`5b6ecdf27767cbd1aafda96ecba92069a024c922`

| 文件 | 字节数 | SHA-256 |
| --- | ---: | --- |
| `25g_2026g_hmi_j11.bit` | 4,045,663 | `4DD2EB78924B75C59FCCCE057F498259DB5313661C59232B461AE3EAE501ADEF` |
| `25g_2026g_hmi_j11.xsa` | 841,150 | `1F848289D326ED229101076766660BC64DD8E76E406ECA18B022ECE408A3C14C` |
| `hmi_candidate_app.elf` | 2,203,076 | `7C4B8BE03253911F571FECECB91F530316143D5942A81F32742D9AD430748D16` |
| `ps7_init.tcl` | 34,511 | `3C4A9FF99EC83AB9C4A8CB98B21C3952AF484E809E050AA075FB151642BCB74D` |
| `26G.HMI` | 7,727,893 | `8636C95B4500E266F8FC5F5D08DE8B9B841051F43867E64BADF5EB6249858AF7` |

XSA 内嵌 bit 的 SHA-256 与独立 bit 完全一致。

离线验证边界：Vivado/Vitis 2020.2 构建成功，WNS `+0.752 ns`、WHS
`+0.035 ns`，DRC 0 Error，构建日志 0 Critical Warning/0 Error。继承设计仍有
非 UART CDC Critical，不能宣称整个系统 CDC clean。最终 bit/XSA/ELF 尚未完成
与实际 TJC 的板级验收；请按 `Doc/26G_HMI_J11_BOARD_TEST.md` 留证后再交队长。

本次交付前没有连接 AX7020、JTAG、串口屏或 CP210x，板级测试尚未开始；这不是
工程或硬件枚举失败。连接设备并完成 F16 电平检查后，再执行上板步骤。
