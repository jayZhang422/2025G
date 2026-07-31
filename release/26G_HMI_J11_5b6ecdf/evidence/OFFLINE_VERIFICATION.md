# 最终离线验证摘要

## 对应关系

- 逻辑源码提交：`5b6ecdf27767cbd1aafda96ecba92069a024c922`
- Vivado/Vitis：2020.2
- FPGA：`xc7z020clg400-2`
- 最终屏幕链路：J11/F17-F16，AXI UARTLite，115200 8N1
- DAC：无外部 DAC 端口；`H_top` 内部旧接口仅为兼容保留
- `BOOT.BIN`：未生成

| 制品 | 字节数 | SHA-256 |
| --- | ---: | --- |
| `25g_2026g_hmi_j11.bit` | 4,045,663 | `4DD2EB78924B75C59FCCCE057F498259DB5313661C59232B461AE3EAE501ADEF` |
| `25g_2026g_hmi_j11.xsa` | 841,150 | `1F848289D326ED229101076766660BC64DD8E76E406ECA18B022ECE408A3C14C` |
| `hmi_candidate_app.elf` | 2,203,076 | `7C4B8BE03253911F571FECECB91F530316143D5942A81F32742D9AD430748D16` |
| `ps7_init.tcl` | 34,511 | `3C4A9FF99EC83AB9C4A8CB98B21C3952AF484E809E050AA075FB151642BCB74D` |
| `26G.HMI` | 7,727,893 | `8636C95B4500E266F8FC5F5D08DE8B9B841051F43867E64BADF5EB6249858AF7` |

XSA 内嵌 bit 与独立 bit 的 SHA-256 相同。

## PL 结果

- 独立 UARTLite BD：`PL_HMI_UART_BD_VALIDATION_PASSED`
- 活动工程集成：`PL_HMI_UART_25G_INTEGRATION_PASSED`
- 完整实现：`PL_HMI_UART_CANDIDATE_BUILD_PASSED WNS=0.752 WHS=0.035`
- 四份 Vivado/Vitis 构建日志：0 Critical Warning，0 Error
- 时序：WNS `+0.752 ns`，WHS `+0.035 ns`，TNS/THS 为 0
- DRC：0 Error，117 Warning，2 Advisory
- bus skew：4 条约束均通过，最小 slack `+8.337 ns`
- I/O：F16=`i_hmi_uart_rx`/INPUT/LVCMOS33；F17=`o_hmi_uart_tx`/OUTPUT/LVCMOS33

CDC 报告仍包含继承设计的 Critical 项，不能把完整工程写成 CDC clean。UART RX
同步器只对应一条 `CDC-3 Info`。

## PS/BSP 结果

- UARTLite device 0，`0x43C30000..0x43C3FFFF`
- 115200，8 data bits，无 parity
- stdin/stdout 保持 PS UART1 `0xE0001000`
- FreeRTOS heap 为 65,536 bytes
- ELF text/data/BSS 为 `1071800 / 3432 / 342848` bytes
- host 协议、会话、坐标和波形重采样回归通过

## 尚未通过的边界

这些结果是离线构建和静态/host 验证，不是最终板测。较早候选的一次 J11
F17-to-F16 1024-byte 环回只能证明基础传输路径，不能替代本制品组合的验收。

最终仍需验证：

- 降压后的屏幕 TX 对 F16 安全；
- 最终 bit/XSA/ps7_init/ELF 与 `26G.HMI` 的双向通信；
- START/STOP、重复 START、page2/page3、1/3 周期、参数、频谱和幅值；
- 从触摸 START 到屏幕最终画面稳定小于 2 秒。
