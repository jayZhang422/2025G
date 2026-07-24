# Refresh the local custom-IP repository and regenerate HDL only.
# This script deliberately does not launch simulation, synthesis,
# implementation, or bitstream generation.

proc require_file {path description} {
    if {![file isfile $path]} {
        error "$description not found: $path"
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
    set ad_fifo_candidates {}
    foreach component [glob -nocomplain -types f -directory $ip_repo */component.xml] {
        if {[string match *fifo* [string tolower [file tail [file dirname $component]]]]} {
            lappend ad_fifo_candidates $component
        }
    }
    set ad_fifo_component [pl_automation::unique_file $ad_fifo_candidates "FIFO custom IP component" --ad-fifo-component]
}
require_file $ad_fifo_component "AD/FIFO IP component"

if {[lsearch -exact $argv "--check"] >= 0} {
    open_project $project_file
    set bd_file [pl_automation::bd_file]
    close_project
    puts "CHECK PASSED: $project_file, $ad_fifo_component, and $bd_file are available."
    return
}

# Update the custom package checksum after its RTL port list changed.
set ad_fifo_core [ipx::open_core $ad_fifo_component]
ipx::update_checksums $ad_fifo_core
ipx::save_core $ad_fifo_core

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
        if {[get_property VLNV $ip] eq $ad_fifo_vlnv} { lappend ad_fifo_candidates $ip }
    }
    set ad_fifo_ip [pl_automation::unique_file $ad_fifo_candidates "AD/FIFO IP" --ad-fifo-ip]
} elseif {[get_ips -quiet $ad_fifo_ip] eq ""} {
    error "AD/FIFO IP is not part of the project: $ad_fifo_ip"
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
pl_automation::remember
close_project
puts "REFRESH PASSED: local IP repository, DDS clock metadata, BD products, and generated wrapper are current."
