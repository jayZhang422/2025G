# J11 PIN3：FPGA TX -> 串口屏 RX
set_property PACKAGE_PIN F17 [get_ports o_hmi_uart_tx]
set_property IOSTANDARD LVCMOS33 [get_ports o_hmi_uart_tx]

# J11 PIN4：串口屏 TX -> FPGA RX
set_property PACKAGE_PIN F16 [get_ports i_hmi_uart_rx]
set_property IOSTANDARD LVCMOS33 [get_ports i_hmi_uart_rx]