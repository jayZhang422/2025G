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
set_property IOSTANDARD LVCMOS33 [get_ports {i_ad_data[12]}]
set_property IOSTANDARD LVCMOS33 [get_ports {i_ad_data[13]}]
set_property IOSTANDARD LVCMOS33 [get_ports i_clk_50m]
set_property IOSTANDARD LVCMOS33 [get_ports i_rst]
set_property IOSTANDARD LVCMOS33 [get_ports o_ad_clk]
set_property IOSTANDARD LVCMOS33 [get_ports o_ad_oeb]
set_property IOSTANDARD LVCMOS33 [get_ports o_ad_pdn]

set_property PACKAGE_PIN R14 [get_ports {i_ad_data[0]}]
set_property PACKAGE_PIN P14 [get_ports {i_ad_data[1]}]
set_property PACKAGE_PIN Y17 [get_ports {i_ad_data[2]}]
set_property PACKAGE_PIN Y16 [get_ports {i_ad_data[3]}]
set_property PACKAGE_PIN W15 [get_ports {i_ad_data[4]}]
set_property PACKAGE_PIN V15 [get_ports {i_ad_data[5]}]
set_property PACKAGE_PIN Y14 [get_ports {i_ad_data[6]}]
set_property PACKAGE_PIN W14 [get_ports {i_ad_data[7]}]
set_property PACKAGE_PIN P18 [get_ports {i_ad_data[8]}]
set_property PACKAGE_PIN N17 [get_ports {i_ad_data[9]}]
set_property PACKAGE_PIN U15 [get_ports {i_ad_data[10]}]
set_property PACKAGE_PIN U14 [get_ports {i_ad_data[11]}]
set_property PACKAGE_PIN P16 [get_ports {i_ad_data[12]}]
set_property PACKAGE_PIN P15 [get_ports {i_ad_data[13]}]
set_property PACKAGE_PIN W18 [get_ports o_ad_clk]
set_property PACKAGE_PIN W19 [get_ports o_ad_oeb]
set_property PACKAGE_PIN U17 [get_ports o_ad_pdn]
set_property PACKAGE_PIN U18 [get_ports i_clk_50m]
set_property PACKAGE_PIN N15 [get_ports i_rst]

# Constrain the FIFO-driving registers, not replicated observation paths.
set_property IOB TRUE [get_cells -hierarchical -regexp \
    {^.*/u_ad9226/dout_reg\[[0-9]+\]$}]

################################################################################
# AD9248 multiplexed-data input timing
#
# AD9248 Rev. B Table 4 specifies a 2 ns minimum and 6 ns maximum data-output
# delay from each clock transition. The selected channel is launched by the
# rising edge and captured just after the following falling edge. Its setup
# limit is therefore the rising-edge maximum. Its hold limit is the earliest
# update after that falling edge.
# These values do not include unknown board clock-to-data trace skew; add that
# measured skew to the min/max delays before final board-level timing signoff.
################################################################################

set_input_delay -clock [get_clocks -of_objects [get_ports o_ad_clk]] \
    -reference_pin [get_ports o_ad_clk] -max 6.000 \
    [get_ports {i_ad_data[*]}]

# Vivado pairs a hold check with the next falling edge. At 65 MHz, subtracting
# one 15.384615 ns period maps that edge back to the physical falling edge
# immediately before capture: 2.000 - 15.384615 = -13.384615 ns.
set_input_delay -clock [get_clocks -of_objects [get_ports o_ad_clk]] \
    -reference_pin [get_ports o_ad_clk] -clock_fall -add_delay \
    -min -13.384615 [get_ports {i_ad_data[*]}]
