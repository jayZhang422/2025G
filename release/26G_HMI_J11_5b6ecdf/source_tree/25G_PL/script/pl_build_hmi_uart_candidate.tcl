# Build the current /3-FIR design with the polling-only J11 HMI UART.
# Artifacts are written outside the repository; top.xsa is never overwritten.

proc require_file {path description} {
    if {![file isfile $path]} {
        error "$description not found: $path"
    }
}

proc require_same_interface_net {left right description} {
    set left_net [get_bd_intf_nets -quiet -of_objects [get_bd_intf_pins $left]]
    set right_net [get_bd_intf_nets -quiet -of_objects [get_bd_intf_pins $right]]
    if {$left_net eq "" || $left_net ne $right_net} {
        error "$description is not connected: $left -> $right"
    }
}

proc require_same_signal_net {left right description} {
    set left_net [get_bd_nets -quiet -of_objects [get_bd_pins $left]]
    set right_net [get_bd_nets -quiet -of_objects [get_bd_pins $right]]
    if {$left_net eq "" || $left_net ne $right_net} {
        error "$description is not connected: $left -> $right"
    }
}

if {![string match "2020.2*" [version -short]]} {
    error "J11 HMI candidate build requires Vivado 2020.2; found [version -short]"
}
if {![info exists ::env(PL_HMI_UART_CANDIDATE_DIR)] ||
    $::env(PL_HMI_UART_CANDIDATE_DIR) eq ""} {
    error "Set PL_HMI_UART_CANDIDATE_DIR to an independent output directory"
}

set script_dir [file dirname [file normalize [info script]]]
set project_dir [file normalize [file join $script_dir ..]]
set project_file [file join $project_dir 25G_PL.xpr]
set bd_file [file join $project_dir 25G_PL.srcs sources_1 bd system system.bd]
set wrapper_file [file join $project_dir 25G_PL.gen sources_1 bd system hdl system_wrapper.v]
set candidate_dir [file normalize $::env(PL_HMI_UART_CANDIDATE_DIR)]
set jobs 8

if {[info exists ::env(PL_HMI_UART_BUILD_JOBS)] &&
    [string is integer -strict $::env(PL_HMI_UART_BUILD_JOBS)] &&
    $::env(PL_HMI_UART_BUILD_JOBS) > 0} {
    set jobs $::env(PL_HMI_UART_BUILD_JOBS)
}

set project_prefix [string tolower [file normalize [file join $project_dir .]]]
set candidate_prefix [string tolower [file normalize [file join $candidate_dir .]]]
if {$candidate_prefix eq $project_prefix ||
    [string first "${project_prefix}/" "${candidate_prefix}/"] == 0} {
    error "Candidate output directory must be outside the source project: $candidate_dir"
}
if {[file exists $candidate_dir] &&
    [llength [glob -nocomplain -directory $candidate_dir *]] != 0} {
    error "Candidate output directory is not empty: $candidate_dir"
}
file mkdir $candidate_dir
require_file $project_file "Vivado project"
require_file $bd_file "Active Block Design"
require_file $wrapper_file "Generated system wrapper"

open_project $project_file
set wrapper_object [get_files -quiet $wrapper_file]
if {$wrapper_object eq "" || ![get_property IS_ENABLED $wrapper_object]} {
    error "Generated system wrapper is not active in sources_1"
}

open_bd_design [get_files $bd_file]
set interconnect [get_bd_cells -quiet axi_interconnect_0]
set uart_core [get_bd_cells -quiet pl_hmi_uart_0/axi_uartlite_core]
set ps [get_bd_cells -quiet processing_system7_0]
if {$interconnect eq "" || $uart_core eq "" || $ps eq ""} {
    error "Required PS/interconnect/UARTLite cells are absent from system.bd"
}
if {[get_property CONFIG.NUM_MI $interconnect] ne "6"} {
    error "Expected axi_interconnect_0 NUM_MI=6"
}

foreach {master target} [list \
    M00_AXI axi_dma_adc/S_AXI_LITE \
    M01_AXI axi_bram_ctrl_0/S_AXI \
    M02_AXI iq_demodulator_0/s_axi \
    M03_AXI ad_fifo_monitor_axi_0/ad_fifo_monitor_axi \
    M04_AXI ddc_stream_0/s_axi \
    M05_AXI pl_hmi_uart_0/S_AXI \
] {
    require_same_interface_net axi_interconnect_0/$master $target \
        "AXI control target $master"
}

require_same_signal_net axi_dma_adc/s2mm_introut xlconcat_0/In0 \
    "DMA S2MM interrupt"
require_same_signal_net iq_demodulator_0/o_irq xlconcat_0/In1 \
    "IQ interrupt"
if {[get_bd_nets -quiet -of_objects [get_bd_pins pl_hmi_uart_0/interrupt]] ne ""} {
    error "UARTLite interrupt must remain unconnected outside its hierarchy"
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

set address_segment [get_bd_addr_segs -quiet \
    processing_system7_0/Data/SEG_axi_uartlite_core_Reg]
if {$address_segment eq ""} {
    error "UARTLite address segment is missing"
}
set uart_offset [expr {[get_property OFFSET $address_segment]}]
set uart_range [expr {[get_property RANGE $address_segment]}]
if {$uart_offset != 0x43C30000 || $uart_range != 0x00010000} {
    error "UARTLite address must be 0x43C30000 with a 64K range"
}

validate_bd_design
generate_target all [get_files $bd_file] -force
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
foreach {port expected_direction expected_pin} [list \
    i_hmi_uart_rx IN F16 \
    o_hmi_uart_tx OUT F17 \
] {
    set port_object [get_ports -quiet $port]
    if {[llength $port_object] != 1} {
        error "Expected exactly one implemented J11 port: $port"
    }
    if {[get_property DIRECTION $port_object] ne $expected_direction ||
        [get_property PACKAGE_PIN $port_object] ne $expected_pin ||
        [get_property IOSTANDARD $port_object] ne "LVCMOS33"} {
        error "Implemented J11 constraint mismatch for $port"
    }
}
report_timing_summary -file [file join $candidate_dir timing_summary.rpt]
report_drc -file [file join $candidate_dir drc.rpt]
report_cdc -details -file [file join $candidate_dir cdc.rpt]
report_utilization -file [file join $candidate_dir utilization.rpt]
report_io -file [file join $candidate_dir io.rpt]
report_bus_skew -file [file join $candidate_dir bus_skew.rpt]
set check_handle [open [file join $candidate_dir check_timing.rpt] w]
puts $check_handle [check_timing -return_string]
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
set output_bit [file join $candidate_dir 25g_2026g_hmi_j11.bit]
set output_xsa [file join $candidate_dir 25g_2026g_hmi_j11.xsa]
require_file $source_bit "Implemented bitstream"
file copy -force $source_bit $output_bit
write_hw_platform -fixed -include_bit -force -file $output_xsa

puts "PL_HMI_UART_CANDIDATE_BUILD_PASSED WNS=$wns WHS=$whs"
puts "PL_HMI_UART_BIT=$output_bit"
puts "PL_HMI_UART_XSA=$output_xsa"
close_project
