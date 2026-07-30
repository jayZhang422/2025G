################################################################################
# AX7020 J11 PL HMI UART
#
# The legacy filename remains active in the project so the constraints file
# set does not need to be renamed. The former external DAC boundary is retired.
################################################################################

set_property PACKAGE_PIN F17 [get_ports o_hmi_uart_tx]
set_property IOSTANDARD LVCMOS33 [get_ports o_hmi_uart_tx]

set_property PACKAGE_PIN F16 [get_ports i_hmi_uart_rx]
set_property IOSTANDARD LVCMOS33 [get_ports i_hmi_uart_rx]
