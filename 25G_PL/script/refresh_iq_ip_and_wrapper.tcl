# Refresh the local custom-IP repository and regenerate HDL only.
# This script deliberately does not launch simulation, synthesis,
# implementation, or bitstream generation.

proc require_file {path description} {
    if {![file isfile $path]} {
        error "$description not found: $path"
    }
}

proc ensure_bus_parameter {bus_interface name value} {
    set parameter [ipx::get_bus_parameters $name -of_objects $bus_interface]
    if {$parameter eq ""} {
        set parameter [ipx::add_bus_parameter $name $bus_interface]
    }
    set_property value $value $parameter
}

proc ensure_clock_interface {core name frequency_hz reset_name} {
    set bus_interface [ipx::get_bus_interfaces $name -of_objects $core]
    if {$bus_interface eq ""} {
        set bus_interface [ipx::add_bus_interface $name $core]
        set_property abstraction_type_vlnv xilinx.com:signal:clock_rtl:1.0 $bus_interface
        set_property bus_type_vlnv xilinx.com:signal:clock:1.0 $bus_interface
        set_property interface_mode slave $bus_interface
        set port_map [ipx::add_port_map CLK $bus_interface]
        set_property physical_name $name $port_map
    }
    ensure_bus_parameter $bus_interface FREQ_HZ $frequency_hz
    ensure_bus_parameter $bus_interface ASSOCIATED_RESET $reset_name
}

proc ensure_input_port {name} {
    set port [get_bd_ports -quiet $name]
    if {$port eq ""} {
        set port [create_bd_port -dir I $name]
    }
    return $port
}

proc connect_scalar_if_needed {source sink} {
    if {[get_bd_nets -quiet -of_objects $sink] eq ""} {
        connect_bd_net $source $sink
    }
}

set script_dir [file dirname [file normalize [info script]]]
source [file join $script_dir pl_automation_helpers.tcl]
pl_automation::init $script_dir
set project_dir [file normalize [file join $script_dir ..]]
set project_file [pl_automation::project_file $project_dir]
set project_dir [file dirname $project_file]
set project_name [pl_automation::project_name $project_file]
set ip_repo [file join $project_dir ip_core]

require_file $project_file "Vivado project"
set ad_fifo_component [pl_automation::option --ad-fifo-component]
if {$ad_fifo_component eq ""} {
    set ad_fifo_component [file join $ip_repo ad_fifo_ip component.xml]
}
require_file $ad_fifo_component "AD/FIFO IP component"
set fifo_monitor_component [pl_automation::option --fifo-monitor-component]
if {$fifo_monitor_component eq ""} {
    set fifo_monitor_component [file join $ip_repo ad_fifo_monitor_axis ad_fifo_monitor_axi_1.0 component.xml]
}
require_file $fifo_monitor_component "FIFO monitor IP component"

if {[lsearch -exact $argv "--check"] >= 0} {
    open_project $project_file
    set bd_file [pl_automation::bd_file]
    close_project
    puts "CHECK PASSED: $project_file, both FIFO components, and $bd_file are available."
    return
}

# Update the custom package checksum after its RTL port list changed.
set ad_fifo_core [ipx::open_core $ad_fifo_component]
ensure_clock_interface $ad_fifo_core clk_phase 65000000 rst_n
ensure_clock_interface $ad_fifo_core rd_clk 100000000 rst_n
ipx::update_checksums $ad_fifo_core
ipx::save_core $ad_fifo_core
set fifo_monitor_core [ipx::open_core $fifo_monitor_component]
ensure_clock_interface $fifo_monitor_core adc_clk 65000000 ""
ipx::update_checksums $fifo_monitor_core
ipx::save_core $fifo_monitor_core
set fifo_monitor_vlnv [get_property VLNV $fifo_monitor_core]

open_project $project_file
set bd_file [pl_automation::bd_file]
set bd_name [file rootname [file tail $bd_file]]
set generated_wrapper [file join $project_dir "$project_name.gen" sources_1 bd $bd_name hdl "${bd_name}_wrapper.v"]
set legacy_wrapper [file join $project_dir "$project_name.srcs" sources_1 imports "${bd_name}_wrapper.v"]
set_property ip_repo_paths [list $ip_repo] [current_project]
update_ip_catalog -rebuild

set dds_ip [pl_automation::choice --dds-ip]
if {$dds_ip eq ""} {
    set dds_candidates {}
    foreach ip [get_ips -quiet] {
        if {[string match *dds_compiler* [get_property IPDEF $ip]]} { lappend dds_candidates $ip }
    }
    set dds_ip [pl_automation::unique_file $dds_candidates "DDS Compiler IP" --dds-ip]
} elseif {[get_ips -quiet $dds_ip] eq ""} {
    error "DDS IP is not part of the project: $dds_ip"
}

# This is metadata for the actual ADC clock domain. Runtime LO frequency
# remains programmable through the IQ core's PINC/POFFSET AXI registers.
set_property CONFIG.ACLK_INTF.FREQ_HZ 5120060 $dds_ip
set_property CONFIG.DDS_Clock_Rate 5.12006 $dds_ip
generate_target all $dds_ip -force

set ad_fifo_ip [pl_automation::choice --ad-fifo-ip]
if {$ad_fifo_ip eq ""} {
    set ad_fifo_candidates {}
    set ad_fifo_vlnv [get_property VLNV $ad_fifo_core]
    foreach ip [get_ips -quiet] {
        if {[get_property IPDEF $ip] eq $ad_fifo_vlnv} { lappend ad_fifo_candidates $ip }
    }
    set ad_fifo_ip [pl_automation::unique_file $ad_fifo_candidates "AD/FIFO IP" --ad-fifo-ip]
} elseif {[get_ips -quiet $ad_fifo_ip] eq ""} {
    error "AD/FIFO IP is not part of the project: $ad_fifo_ip"
}
upgrade_ip $ad_fifo_ip
generate_target all $ad_fifo_ip -force

open_bd_design $bd_file

# The monitor is an AXI4-Lite peripheral inside the BD so PS software can
# request coherent snapshots and read the published counters.
set fifo_monitor_cell [get_bd_cells -quiet fifo_monitor_axi_0]
if {$fifo_monitor_cell eq ""} {
    set fifo_monitor_cell [create_bd_cell -type ip -vlnv $fifo_monitor_vlnv fifo_monitor_axi_0]
} else {
    upgrade_bd_cells $fifo_monitor_cell
    set fifo_monitor_cell [get_bd_cells fifo_monitor_axi_0]
}

set control_interconnect [get_bd_cells axi_interconnect_0]
if {[get_property CONFIG.NUM_MI $control_interconnect] < 4} {
    set_property CONFIG.NUM_MI 4 $control_interconnect
}
if {[get_bd_intf_nets -quiet -of_objects [get_bd_intf_pins ${fifo_monitor_cell}/ad_fifo_monitor_axi]] eq ""} {
    connect_bd_intf_net [get_bd_intf_pins axi_interconnect_0/M03_AXI] \
        [get_bd_intf_pins ${fifo_monitor_cell}/ad_fifo_monitor_axi]
}

connect_scalar_if_needed [get_bd_pins processing_system7_0/FCLK_CLK0] \
    [get_bd_pins axi_interconnect_0/M03_ACLK]
connect_scalar_if_needed [get_bd_pins proc_sys_reset_0/peripheral_aresetn] \
    [get_bd_pins axi_interconnect_0/M03_ARESETN]
connect_scalar_if_needed [get_bd_pins processing_system7_0/FCLK_CLK0] \
    [get_bd_pins ${fifo_monitor_cell}/ad_fifo_monitor_axi_aclk]
connect_scalar_if_needed [get_bd_pins proc_sys_reset_0/peripheral_aresetn] \
    [get_bd_pins ${fifo_monitor_cell}/ad_fifo_monitor_axi_aresetn]
connect_scalar_if_needed [get_bd_ports clk_adc_in] \
    [get_bd_pins ${fifo_monitor_cell}/adc_clk]
connect_scalar_if_needed [get_bd_ports i_sample_valid_0] \
    [get_bd_pins ${fifo_monitor_cell}/sample_valid]

foreach {port_name pin_name} {
    fifo_mon_write        fifo_write
    fifo_mon_prog_full    fifo_prog_full
    fifo_mon_full         fifo_full
    fifo_mon_wr_rst_busy  fifo_wr_rst_busy
    fifo_mon_rd_rst_busy  fifo_rd_rst_busy
    monitor_axis_tvalid   axis_tvalid
    monitor_axis_tready   axis_tready
    monitor_axis_tlast    axis_tlast
} {
    connect_scalar_if_needed [ensure_input_port $port_name] \
        [get_bd_pins ${fifo_monitor_cell}/$pin_name]
}

assign_bd_address -offset 0x43C10000 -range 64K \
    -target_address_space [get_bd_addr_spaces processing_system7_0/Data] \
    [get_bd_addr_segs ${fifo_monitor_cell}/ad_fifo_monitor_axi/ad_fifo_monitor_axi_reg] -force
save_bd_design

if {[catch {validate_bd_design} validation_error]} {
    error "Block Design validation failed: $validation_error"
}
generate_target all [get_files $bd_file] -force
make_wrapper -files [get_files $bd_file] -top -force
require_file $generated_wrapper "Generated HDL wrapper"

# Keep the old import on disk as a recovery copy but remove it from sources_1.
set legacy_file [get_files -quiet $legacy_wrapper]
if {$legacy_file ne ""} {
    remove_files $legacy_file
}
if {[get_files -quiet $generated_wrapper] eq ""} {
    add_files -norecurse $generated_wrapper
}

update_compile_order -fileset sources_1
pl_automation::remember
close_project
puts "REFRESH PASSED: local IP repository, AXI FIFO monitor, DDS clock metadata, BD products, and generated wrapper are current."
