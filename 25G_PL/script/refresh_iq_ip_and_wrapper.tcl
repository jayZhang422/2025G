# Refresh the local custom-IP repository and regenerate HDL only.
# This script deliberately does not launch simulation, synthesis,
# implementation, or bitstream generation.

proc require_file {path description} {
    if {![file isfile $path]} {
        error "$description not found: $path"
    }
}

set script_dir [file dirname [file normalize [info script]]]
set project_dir [file normalize [file join $script_dir ..]]
set project_file [file join $project_dir 25G_PL.xpr]
set ip_repo [file join $project_dir ip_core]
set ad_fifo_component [file join $ip_repo ad_fifo_ip component.xml]
set bd_file [file join $project_dir 25G_PL.srcs sources_1 bd system system.bd]
set generated_wrapper [file join $project_dir 25G_PL.gen sources_1 bd system hdl system_wrapper.v]
set legacy_wrapper [file join $project_dir 25G_PL.srcs sources_1 imports system_wrapper.v]

require_file $project_file "Vivado project"
require_file $ad_fifo_component "AD/FIFO IP component"
require_file $bd_file "Block Design"

# Update the custom package checksum after its RTL port list changed.
set ad_fifo_core [ipx::open_core $ad_fifo_component]
ipx::update_checksums $ad_fifo_core
ipx::save_core $ad_fifo_core

open_project $project_file
set_property ip_repo_paths [list $ip_repo] [current_project]
update_ip_catalog -rebuild

set dds_ip [get_ips -quiet dds_iq_lo]
if {$dds_ip eq ""} {
    error "dds_iq_lo is not part of the project"
}

# This is metadata for the actual ADC clock domain. Runtime LO frequency
# remains programmable through the IQ core's PINC/POFFSET AXI registers.
set_property CONFIG.ACLK_INTF.FREQ_HZ 5120060 $dds_ip
set_property CONFIG.DDS_Clock_Rate 5.12006 $dds_ip
generate_target all $dds_ip -force

set ad_fifo_ip [get_ips -quiet ad_fifo_output]
if {$ad_fifo_ip eq ""} {
    error "ad_fifo_output is not part of the project"
}
upgrade_ip $ad_fifo_ip
generate_target all $ad_fifo_ip -force

open_bd_design $bd_file
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
close_project
puts "REFRESH PASSED: local IP repository, DDS clock metadata, BD products, and generated wrapper are current."
