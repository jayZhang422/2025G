# Integrate a dedicated PS-write/DDS-read waveform BRAM into the active BD.
# Run from 25G_PL: vivado -mode batch -source script/pl_integrate_wave_ram.tcl

proc require_file {path description} {
    if {![file isfile $path]} {
        error "$description not found: $path"
    }
}

proc progress {message} {
    puts "[clock format [clock seconds] -format {%Y-%m-%d %H:%M:%S}] WAVE RAM: $message"
    flush stdout
}

set script_dir [file dirname [file normalize [info script]]]
set project_dir [file normalize [file join $script_dir ..]]
set project_file [file join $project_dir 25G_PL.xpr]
set bd_file [file join $project_dir 25G_PL.srcs sources_1 bd system system.bd]
set adapter_file [file join $project_dir 25G_PL.srcs sources_1 new g2025_dac_adapter.sv]
set dds_file [file join $project_dir ip_core DDS_DAC_ip DAC_DDS_Output.sv]

foreach {path description} [list \
    $project_file "Vivado project" \
    $bd_file "Block Design" \
    $adapter_file "G2025 DAC adapter" \
    $dds_file "Packaged DDS core"] {
    require_file $path $description
}

progress "opening project"
open_project $project_file
open_bd_design $bd_file

if {[get_bd_cells -quiet axi_bram_ctrl_wave] ne "" ||
    [get_bd_cells -quiet blk_PS_TO_PL_WAVE] ne "" ||
    [get_bd_intf_ports -quiet WAVE_RAM] ne ""} {
    error "Waveform RAM integration already exists; no changes were made."
}

progress "creating dedicated waveform BRAM and AXI controller"
set WAVE_RAM [create_bd_intf_port -mode Slave -vlnv xilinx.com:interface:bram_rtl:1.0 WAVE_RAM]
set_property -dict [list CONFIG.MASTER_TYPE {BRAM_CTRL}] $WAVE_RAM

set axi_bram_ctrl_wave [create_bd_cell -type ip -vlnv xilinx.com:ip:axi_bram_ctrl:4.1 axi_bram_ctrl_wave]
set_property -dict [list CONFIG.ECC_TYPE {0} CONFIG.PROTOCOL {AXI4LITE} CONFIG.SINGLE_PORT_BRAM {1}] $axi_bram_ctrl_wave

set blk_PS_TO_PL_WAVE [create_bd_cell -type ip -vlnv xilinx.com:ip:blk_mem_gen:8.4 blk_PS_TO_PL_WAVE]
set_property -dict [list \
    CONFIG.Memory_Type {True_Dual_Port_RAM} \
    CONFIG.Write_Width_A {32} \
    CONFIG.Read_Width_A {32} \
    CONFIG.Write_Depth_A {2048} \
    CONFIG.Read_Depth_A {2048} \
    CONFIG.Write_Width_B {16} \
    CONFIG.Read_Width_B {16} \
    CONFIG.Write_Depth_B {4096} \
    CONFIG.Read_Depth_B {4096} \
    CONFIG.Enable_B {Use_ENB_Pin} \
    CONFIG.Port_B_Clock {125} \
    CONFIG.Port_B_Enable_Rate {100} \
    CONFIG.Port_B_Write_Rate {0} \
    CONFIG.Use_RSTB_Pin {true}] $blk_PS_TO_PL_WAVE

progress "connecting AXI, BRAM, clock, and reset"
set_property CONFIG.NUM_MI {3} [get_bd_cells axi_interconnect_0]
connect_bd_intf_net [get_bd_intf_ports WAVE_RAM] [get_bd_intf_pins blk_PS_TO_PL_WAVE/BRAM_PORTB]
connect_bd_intf_net [get_bd_intf_pins axi_bram_ctrl_wave/BRAM_PORTA] [get_bd_intf_pins blk_PS_TO_PL_WAVE/BRAM_PORTA]
connect_bd_intf_net [get_bd_intf_pins axi_interconnect_0/M02_AXI] [get_bd_intf_pins axi_bram_ctrl_wave/S_AXI]
connect_bd_net [get_bd_pins proc_sys_reset_0/peripheral_aresetn] [get_bd_pins axi_bram_ctrl_wave/s_axi_aresetn]
connect_bd_net [get_bd_pins processing_system7_0/FCLK_CLK0] [get_bd_pins axi_bram_ctrl_wave/s_axi_aclk]

assign_bd_address -offset 0x42000000 -range 0x00002000 \
    -target_address_space [get_bd_addr_spaces processing_system7_0/Data] \
    -target_address_segs [get_bd_addr_segs axi_bram_ctrl_wave/S_AXI/Mem0] -force

progress "validating Block Design"
validate_bd_design
save_bd_design
generate_target all [get_files $bd_file] -force
make_wrapper -files [get_files $bd_file] -top -force

foreach source_file [list $adapter_file $dds_file] {
    if {[get_files -quiet $source_file] eq ""} {
        add_files -norecurse $source_file
    }
}
update_compile_order -fileset sources_1
save_project
close_project
progress "DONE: dedicated waveform RAM integrated at 0x42000000"
