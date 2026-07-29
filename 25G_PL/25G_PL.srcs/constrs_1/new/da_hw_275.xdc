################################################################################
# Legacy filename retained to avoid renaming an existing project interface file.
#
# The final integration does not use the DAC. The former DAC pin assignments
# are retired; this active board-constraint slot now owns only the PL HMI UART
# on AX7020 J11 BANK35 (3.3 V).
#
# J11 pin 3 / F17: FPGA TX to display RX
# J11 pin 4 / F16: FPGA RX from display TX
# J11 pin 1: signal ground
################################################################################

set_property PACKAGE_PIN F17 [get_ports o_hmi_uart_tx]
set_property IOSTANDARD LVCMOS33 [get_ports o_hmi_uart_tx]

set_property PACKAGE_PIN F16 [get_ports i_hmi_uart_rx]
set_property IOSTANDARD LVCMOS33 [get_ports i_hmi_uart_rx]
