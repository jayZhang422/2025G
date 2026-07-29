# Build and export the active 25G PL HMI UART candidate without overwriting
# the repository's existing top.xsa or invoking any boot-image flow.

proc require_file {path description} {
    if {![file isfile $path]} {
        error "$description not found: $path"
    }
}

if {![string match "2020.2*" [version -short]]} {
    error "25G UART candidate build requires Vivado 2020.2; found [version -short]"
}
if {![info exists ::env(PL_HMI_UART_CANDIDATE_DIR)] ||
    $::env(PL_HMI_UART_CANDIDATE_DIR) eq ""} {
    error "Set PL_HMI_UART_CANDIDATE_DIR to an independent output directory"
}

set script_dir [file dirname [file normalize [info script]]]
set repo_root [file normalize [file join $script_dir .. .. ..]]
set project_file [file join $repo_root 25G_PL 25G_PL.xpr]
set wrapper_file [file join $repo_root 25G_PL 25G_PL.gen sources_1 bd system hdl system_wrapper.v]
set candidate_dir [file normalize $::env(PL_HMI_UART_CANDIDATE_DIR)]
set jobs 8

file mkdir $candidate_dir
require_file $project_file "Vivado project"
require_file $wrapper_file "Generated system wrapper"

open_project $project_file
set wrapper_object [get_files -quiet $wrapper_file]
if {$wrapper_object eq "" || ![get_property IS_ENABLED $wrapper_object]} {
    error "Generated system wrapper is not active in sources_1"
}

open_bd_design [get_files system.bd]
set uart_core [get_bd_cells -quiet pl_hmi_uart_0/axi_uartlite_core]
set ps [get_bd_cells -quiet processing_system7_0]
if {$uart_core eq "" || $ps eq ""} {
    error "UARTLite hierarchy or PS7 is absent from system.bd"
}
foreach {object property expected} [list \
    $uart_core CONFIG.C_BAUDRATE {115200} \
    $uart_core CONFIG.C_DATA_BITS {8} \
    $uart_core CONFIG.C_USE_PARITY {0} \
    $uart_core CONFIG.C_S_AXI_ACLK_FREQ_HZ {100000000} \
    $ps CONFIG.PCW_EN_SDIO0 {1} \
    $ps CONFIG.PCW_SD0_PERIPHERAL_ENABLE {1} \
    $ps CONFIG.PCW_SD0_SD0_IO {MIO 40 .. 45} \
    $ps CONFIG.PCW_SD0_GRP_CD_ENABLE {1} \
    $ps CONFIG.PCW_SD0_GRP_CD_IO {MIO 47} \
] {
    set actual [get_property $property $object]
    if {$actual ne $expected} {
        error "Candidate assertion failed: $property expected '$expected' got '$actual'"
    }
}
validate_bd_design
generate_target all [get_files system.bd] -force
update_compile_order -fileset sources_1

if {[get_property TOP [get_filesets sources_1]] ne "top"} {
    error "Expected synthesis top 'top'"
}
set_param general.maxThreads $jobs
reset_runs impl_1
reset_runs synth_1
launch_runs synth_1 -jobs $jobs
wait_on_run synth_1
if {[get_property STATUS [get_runs synth_1]] ne "synth_design Complete!"} {
    error "Synthesis failed: [get_property STATUS [get_runs synth_1]]"
}

launch_runs impl_1 -to_step write_bitstream -jobs $jobs
wait_on_run impl_1
if {[get_property STATUS [get_runs impl_1]] ne "write_bitstream Complete!"} {
    error "Implementation failed: [get_property STATUS [get_runs impl_1]]"
}

open_run impl_1
report_timing_summary -file [file join $candidate_dir timing_summary.rpt]
report_drc -file [file join $candidate_dir drc.rpt]
report_cdc -details -file [file join $candidate_dir cdc.rpt]
report_utilization -file [file join $candidate_dir utilization.rpt]
report_io -file [file join $candidate_dir io.rpt]
report_bus_skew -file [file join $candidate_dir bus_skew.rpt]
set check_timing_text [check_timing -return_string]
set check_handle [open [file join $candidate_dir check_timing.rpt] w]
puts $check_handle $check_timing_text
close $check_handle

set setup_path [lindex [get_timing_paths -delay_type max -max_paths 1] 0]
set hold_path [lindex [get_timing_paths -delay_type min -max_paths 1] 0]
if {$setup_path eq "" || $hold_path eq ""} {
    error "Could not obtain setup/hold timing paths"
}
set wns [get_property SLACK $setup_path]
set whs [get_property SLACK $hold_path]
if {$wns < 0.0 || $whs < 0.0} {
    error "Timing failed: WNS=$wns WHS=$whs"
}

set drc_errors [get_drc_violations -quiet -filter {SEVERITY == Error}]
if {[llength $drc_errors] != 0} {
    error "Implementation has DRC errors: $drc_errors"
}

set implementation_dir [get_property DIRECTORY [get_runs impl_1]]
set source_bit [file join $implementation_dir top.bit]
set output_bit [file join $candidate_dir 25g_pl_hmi_uart.bit]
set output_xsa [file join $candidate_dir 25g_pl_hmi_uart.xsa]
require_file $source_bit "Implemented bitstream"
file copy -force $source_bit $output_bit
write_hw_platform -fixed -include_bit -force -file $output_xsa

puts "PL_HMI_UART_CANDIDATE_BUILD_PASSED WNS=$wns WHS=$whs"
puts "PL_HMI_UART_BIT=$output_bit"
puts "PL_HMI_UART_XSA=$output_xsa"
close_project
