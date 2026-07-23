# Add the reviewed G2025 v3 top path and optionally make it the active XPR top.
# Usage:
#   vivado -mode batch -source pl_select_g2025_top_v3.tcl -tclargs --check
#   vivado -mode batch -source pl_select_g2025_top_v3.tcl -tclargs --compile-only
#   vivado -mode batch -source pl_select_g2025_top_v3.tcl
#
# The legacy top is retained as a source and rollback reference.  The generated
# system_wrapper is required; this script never substitutes the legacy wrapper.

proc require_file {path description} {
    if {![file isfile $path]} {
        error "$description not found: $path"
    }
}

proc has_arg {name} {
    global argv
    return [expr {[lsearch -exact $argv $name] >= 0}]
}

set script_dir [file dirname [file normalize [info script]]]
set project_dir [file normalize [file join $script_dir ..]]
set project_file [file join $project_dir 25G_PL.xpr]
set generated_wrapper [file normalize [file join $project_dir 25G_PL.gen sources_1 bd system hdl system_wrapper.v]]
set reviewed_top [file normalize [file join $project_dir 25G_PL.srcs sources_1 new g2025_top_v3.sv]]
set reviewed_adapter [file normalize [file join $project_dir 25G_PL.srcs sources_1 new g2025_dac_adapter_v3.sv]]
set dds_rtl [file normalize [file join $project_dir ip_core DDS_DAC_ip DAC_DDS_Output.sv]]

require_file $project_file "Vivado project"
require_file $generated_wrapper "generated system wrapper"
require_file $reviewed_top "reviewed G2025 top"
require_file $reviewed_adapter "reviewed G2025 adapter"
require_file $dds_rtl "packaged DDS RTL"

open_project $project_file
set sources [get_filesets sources_1]
set required_files [list $generated_wrapper $reviewed_top $reviewed_adapter $dds_rtl]
foreach path $required_files {
    set existing [get_files -quiet -of_objects $sources $path]
    if {$existing eq ""} {
        add_files -norecurse -fileset $sources $path
    }
}
update_compile_order -fileset $sources

set current_top [get_property top $sources]
set generated_present [expr {[get_files -quiet -of_objects $sources $generated_wrapper] ne ""}]
if {!$generated_present} {
    error "generated system_wrapper is not in sources_1; refusing top change"
}
puts "G2025_REQUIRED_SOURCES_OK"
puts "CURRENT_TOP=$current_top"
puts "GENERATED_WRAPPER=$generated_wrapper"

if {[has_arg --check]} {
    close_project
    exit 0
}

set_property top g2025_top_v3 $sources
update_compile_order -fileset $sources

if {[has_arg --compile-only]} {
    set part [get_property PART [current_project]]
    puts "COMPILE_ONLY_PART=$part"
    synth_design -rtl -top g2025_top_v3 -part $part
    close_project
    exit 0
}

save_project
puts "G2025_TOP_V3_SELECTED"
close_project