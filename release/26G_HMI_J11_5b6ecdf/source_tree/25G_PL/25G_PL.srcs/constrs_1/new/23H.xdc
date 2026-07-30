################################################################################
# 25G board-level user constraints
#
# This file contains only constraints owned by this project. Vivado loads PS7,
# DMA, FIFO, PLL, XPM, and debug-core constraints from their IP output products;
# those generated constraints must not be flattened back into this file.
################################################################################

################################################################################
# AD9226 interface, board clock, and reset
################################################################################

set_property IOSTANDARD LVCMOS33 [get_ports {i_ad_data[0]}]
set_property IOSTANDARD LVCMOS33 [get_ports {i_ad_data[1]}]
set_property IOSTANDARD LVCMOS33 [get_ports {i_ad_data[2]}]
set_property IOSTANDARD LVCMOS33 [get_ports {i_ad_data[3]}]
set_property IOSTANDARD LVCMOS33 [get_ports {i_ad_data[4]}]
set_property IOSTANDARD LVCMOS33 [get_ports {i_ad_data[5]}]
set_property IOSTANDARD LVCMOS33 [get_ports {i_ad_data[6]}]
set_property IOSTANDARD LVCMOS33 [get_ports {i_ad_data[7]}]
set_property IOSTANDARD LVCMOS33 [get_ports {i_ad_data[8]}]
set_property IOSTANDARD LVCMOS33 [get_ports {i_ad_data[9]}]
set_property IOSTANDARD LVCMOS33 [get_ports {i_ad_data[10]}]
set_property IOSTANDARD LVCMOS33 [get_ports {i_ad_data[11]}]
set_property IOSTANDARD LVCMOS33 [get_ports i_clk_50m]
set_property IOSTANDARD LVCMOS33 [get_ports i_rst]
set_property IOSTANDARD LVCMOS33 [get_ports o_ad_clk]

set_property PACKAGE_PIN T15 [get_ports {i_ad_data[0]}]
set_property PACKAGE_PIN U13 [get_ports {i_ad_data[1]}]
set_property PACKAGE_PIN V13 [get_ports {i_ad_data[2]}]
set_property PACKAGE_PIN V12 [get_ports {i_ad_data[3]}]
set_property PACKAGE_PIN W13 [get_ports {i_ad_data[4]}]
set_property PACKAGE_PIN T12 [get_ports {i_ad_data[5]}]
set_property PACKAGE_PIN U12 [get_ports {i_ad_data[6]}]
set_property PACKAGE_PIN T11 [get_ports {i_ad_data[7]}]
set_property PACKAGE_PIN T10 [get_ports {i_ad_data[8]}]
set_property PACKAGE_PIN B19 [get_ports {i_ad_data[9]}]
set_property PACKAGE_PIN A20 [get_ports {i_ad_data[10]}]
set_property PACKAGE_PIN C20 [get_ports {i_ad_data[11]}]
set_property PACKAGE_PIN T14 [get_ports o_ad_clk]
set_property PACKAGE_PIN U18 [get_ports i_clk_50m]
set_property PACKAGE_PIN N15 [get_ports i_rst]


################################################################################
# Active-low PL keys
################################################################################

set_property IOSTANDARD LVCMOS33 [get_ports {pl_key_i[0]}]
set_property IOSTANDARD LVCMOS33 [get_ports {pl_key_i[1]}]
set_property IOSTANDARD LVCMOS33 [get_ports {pl_key_i[2]}]
set_property PULLUP true [get_ports {pl_key_i[0]}]
set_property PULLUP true [get_ports {pl_key_i[1]}]
set_property PULLUP true [get_ports {pl_key_i[2]}]
set_property PACKAGE_PIN N16 [get_ports {pl_key_i[0]}]
set_property PACKAGE_PIN T17 [get_ports {pl_key_i[1]}]
set_property PACKAGE_PIN R17 [get_ports {pl_key_i[2]}]

################################################################################
# ADC FIFO clock-domain crossing
#
# PLL_AD.xdc already creates the 20 ns input clock on its scoped clk_sys port,
# which resolves to top-level i_clk_50m. Keep only that IP-owned definition to
# avoid overriding the scoped clock constraint.
################################################################################

set_false_path -from [get_clocks clk_pll_phase_PLL_AD] -to [get_clocks clk_fpga_0]
set_false_path -from [get_clocks clk_fpga_0] -to [get_clocks clk_pll_phase_PLL_AD]

################################################################################
# DAC board export retired
#
# H_top retains the existing internal DAC interface names for compatibility,
# but top no longer exports those pins. Their former output-delay and IOB
# constraints therefore do not apply to this J11 UART integration.
################################################################################
