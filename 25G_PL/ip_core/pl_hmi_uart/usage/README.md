# J11 PL HMI UARTLite IP

这是最终 J11 串口屏链路的可移植 PL 封装目录，按队友工程的
`25G_PL/ip_core/<module>/usage/README.md` 方式组织。

## 这是什么

这里的文件是 Vivado 2020.2 Block Design 层级封装源码，不是重新实现的 UART
RTL，也不是一个需要伪造 `component.xml` 的自定义 UART 控制器。实际串口核心是
Xilinx 官方 `xilinx.com:ip:axi_uartlite:2.0`。

权威源码：

```text
25G_PL/ip_core/pl_hmi_uart/src/pl_hmi_uart_bd.tcl
```

它定义：

```tcl
create_pl_hmi_uart_subsystem parent_cell instance_name
```

固定参数：115200 baud、8 data bits、无 parity、100 MHz AXI 时钟。

## 在父 BD 中使用

```tcl
source 25G_PL/ip_core/pl_hmi_uart/src/pl_hmi_uart_bd.tcl
create_pl_hmi_uart_subsystem / pl_hmi_uart_0
```

父 BD 负责连接 `S_AXI`、`s_axi_aclk`、`s_axi_aresetn`、`UART`，分配地址并把
`UART` 暴露到顶层。最终工程使用 `axi_interconnect_0/M05_AXI`，地址来自当前
XSA/BSP；不能把历史地址复制到新的工程。

## 最终外部边界

- J11 PIN3 / FPGA F17：UARTLite TX -> 屏幕 RX。
- J11 PIN4 / FPGA F16：屏幕 TX -> UARTLite RX，接入前必须经过已验证的降压/保护。
- J11 PIN1：公共信号地。

J11 约束和父工程连接不放在该 IP 目录中。UART 中断保持未连接，由 PS 轮询驱动。
屏幕不从 J11 取电。本工程不需要 DAC。

## 兼容入口

`25G_PL/script/pl_hmi_uart_bd.tcl` 现在只是兼容入口，会转而加载本目录的权威
源码。新工程请直接引用 `ip_core/pl_hmi_uart/src` 路径，避免把集成脚本和 IP
源码混在一起。
