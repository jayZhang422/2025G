################################################################################
# 25G board-level user constraints
#
# This file contains only constraints owned by this project. Vivado loads PS7,
# DMA, FIFO, PLL, XPM, and debug-core constraints from their IP output products;
# those generated constraints must not be flattened back into this file.
################################################################################


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
# AD9767 forwarded-clock output timing
#
# Values are unchanged from the board-tested baseline. WRT is driven in phase
# with the forwarded DAC clock. Data launches on the preceding falling edge,
# giving a nominal 4 ns setup window at 125 MHz. The delays assume matched
# FPGA-to-module clock/data board routing.
################################################################################

create_generated_clock -name dac_clk_a_forwarded -source [get_pins u_h_top/dac_clk_a_forward/C] -divide_by 1 [get_ports o_da_clk]
create_generated_clock -name dac_wrt_a_forwarded -source [get_pins u_h_top/dac_wrt_a_forward/C] -divide_by 1 [get_ports o_da_wrt]
create_generated_clock -name dac_clk_b_forwarded -source [get_pins u_h_top/dac_clk_b_forward/C] -divide_by 1 [get_ports o_da_clk_b]
create_generated_clock -name dac_wrt_b_forwarded -source [get_pins u_h_top/dac_wrt_b_forward/C] -divide_by 1 [get_ports o_da_wrt_b]

set_property IOB TRUE [get_cells -hierarchical -regexp {.*u_dac_dds(/.*)?/da_data_a_reg\[[0-9]+\]$}]
set_property IOB TRUE [get_cells -hierarchical -regexp {.*u_dac_dds(/.*)?/da_data_b_reg\[[0-9]+\]$}]

set_output_delay -clock dac_clk_a_forwarded -max 2.000 [get_ports {o_da_data[*]}]
set_output_delay -clock dac_clk_a_forwarded -min -1.500 [get_ports {o_da_data[*]}]
set_output_delay -clock dac_wrt_a_forwarded -max -add_delay 2.000 [get_ports {o_da_data[*]}]
set_output_delay -clock dac_wrt_a_forwarded -min -add_delay -1.500 [get_ports {o_da_data[*]}]
set_output_delay -clock dac_clk_b_forwarded -max 2.000 [get_ports {o_da_data_b[*]}]
set_output_delay -clock dac_clk_b_forwarded -min -1.500 [get_ports {o_da_data_b[*]}]
set_output_delay -clock dac_wrt_b_forwarded -max -add_delay 2.000 [get_ports {o_da_data_b[*]}]
set_output_delay -clock dac_wrt_b_forwarded -min -add_delay -1.500 [get_ports {o_da_data_b[*]}]
