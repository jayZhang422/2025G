# V5 completes the new AXI-interconnect clock/reset connection required after
# increasing NUM_MI from two to three.
proc progress {message} { puts "[clock format [clock seconds] -format {%Y-%m-%d %H:%M:%S}] G2025 WAVE: $message"; flush stdout }
set d [file normalize [file join [file dirname [file normalize [info script]]] ..]]
open_project [file join $d 25G_PL.xpr]
set bd [file join $d 25G_PL.srcs sources_1 bd system system.bd]
open_bd_design $bd
if {[get_bd_cells -quiet axi_bram_ctrl_wave] ne ""} { error "waveform RAM already integrated" }
set p [create_bd_intf_port -mode Slave -vlnv xilinx.com:interface:bram_rtl:1.0 WAVE_RAM]
set_property -dict [list CONFIG.MASTER_TYPE {BRAM_CTRL}] $p
set c [create_bd_cell -type ip -vlnv xilinx.com:ip:axi_bram_ctrl:4.1 axi_bram_ctrl_wave]
set_property -dict [list CONFIG.ECC_TYPE {0} CONFIG.PROTOCOL {AXI4LITE} CONFIG.SINGLE_PORT_BRAM {1}] $c
set m [create_bd_cell -type ip -vlnv xilinx.com:ip:blk_mem_gen:8.4 blk_PS_TO_PL_WAVE]
set_property -dict [list CONFIG.Memory_Type {True_Dual_Port_RAM} CONFIG.Write_Width_A {32} CONFIG.Read_Width_A {32} CONFIG.Write_Depth_A {4096} CONFIG.Write_Width_B {32} CONFIG.Read_Width_B {32} CONFIG.Enable_B {Use_ENB_Pin} CONFIG.Port_B_Clock {125} CONFIG.Port_B_Enable_Rate {100} CONFIG.Port_B_Write_Rate {0} CONFIG.Use_RSTB_Pin {true}] $m
set_property CONFIG.NUM_MI {3} [get_bd_cells axi_interconnect_0]
connect_bd_intf_net [get_bd_intf_ports WAVE_RAM] [get_bd_intf_pins blk_PS_TO_PL_WAVE/BRAM_PORTB]
connect_bd_intf_net [get_bd_intf_pins axi_bram_ctrl_wave/BRAM_PORTA] [get_bd_intf_pins blk_PS_TO_PL_WAVE/BRAM_PORTA]
connect_bd_intf_net [get_bd_intf_pins axi_interconnect_0/M02_AXI] [get_bd_intf_pins axi_bram_ctrl_wave/S_AXI]
connect_bd_net [get_bd_pins processing_system7_0/FCLK_CLK0] [get_bd_pins axi_bram_ctrl_wave/s_axi_aclk] [get_bd_pins axi_interconnect_0/M02_ACLK]
connect_bd_net [get_bd_pins proc_sys_reset_0/peripheral_aresetn] [get_bd_pins axi_bram_ctrl_wave/s_axi_aresetn] [get_bd_pins axi_interconnect_0/M02_ARESETN]
assign_bd_address -offset 0x42000000 -range 0x00004000 -target_address_space [get_bd_addr_spaces processing_system7_0/Data] [get_bd_addr_segs axi_bram_ctrl_wave/S_AXI/Mem0] -force
progress "validating BD"
validate_bd_design
save_bd_design
generate_target all [get_files $bd] -force
make_wrapper -files [get_files $bd] -top -force
write_bd_tcl -force [file join $d system.tcl]
save_project
close_project
progress "WAVE RAM BD INTEGRATION PASSED"
