################################################################################
# AX7020 J11 PL HMI UART
#
# The legacy filename remains active in the project so the constraints file
# set does not need to be renamed. The former external DAC boundary is retired.
################################################################################

if {[llength [get_ports -quiet o_hmi_uart_tx]] != 1} {
    error "Expected exactly one o_hmi_uart_tx port before applying J11 TX constraint"
}
if {[llength [get_ports -quiet i_hmi_uart_rx]] != 1} {
    error "Expected exactly one i_hmi_uart_rx port before applying J11 RX constraint"
}

set_property PACKAGE_PIN F17 [get_ports o_hmi_uart_tx]
set_property IOSTANDARD LVCMOS33 [get_ports o_hmi_uart_tx]

set_property PACKAGE_PIN F16 [get_ports i_hmi_uart_rx]
set_property IOSTANDARD LVCMOS33 [get_ports i_hmi_uart_rx]
