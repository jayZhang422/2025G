# Integrate the portable PL HMI UART into the active 25G Vivado 2020.2 project.
# This script intentionally leaves the UART interrupt unconnected for polling.

proc require_file {path description} {
    if {![file isfile $path]} {
        error "$description not found: $path"
    }
}

proc require_bd_cell {name} {
    set cell [get_bd_cells -quiet $name]
    if {$cell eq ""} {
        error "Required Block Design cell not found: $name"
    }
    return $cell
}

if {![string match "2020.2*" [version -short]]} {
    error "25G PL HMI UART integration requires Vivado 2020.2; found [version -short]"
}

set script_dir [file dirname [file normalize [info script]]]
set repo_root [file normalize [file join $script_dir .. .. ..]]
set project_dir [file join $repo_root 25G_PL]
set project_file [file join $project_dir 25G_PL.xpr]
set bd_file [file join $project_dir 25G_PL.srcs sources_1 bd system system.bd]
set wrapper_file [file join $project_dir 25G_PL.gen sources_1 bd system hdl system_wrapper.v]
set bd_tcl_file [file join $project_dir system.tcl]
set uart_source [file join $script_dir pl_hmi_uart_bd.tcl]
set board_xdc [file join $project_dir 25G_PL.srcs constrs_1 new da_hw_275.xdc]

require_file $project_file "Vivado project"
require_file $bd_file "Active Block Design"
require_file $uart_source "Portable UART hierarchy source"
require_file $board_xdc "Legacy-named J11 board constraint"

if {[current_project -quiet] ne ""} {
    error "Close the current Vivado project before running this integration script"
}

open_project $project_file
open_bd_design [get_files $bd_file]
source $uart_source

set interconnect [require_bd_cell axi_interconnect_0]
set ps [require_bd_cell processing_system7_0]
set reset [require_bd_cell proc_sys_reset_0]

# AX7020 final delivery boots from the onboard Micro SD. This configuration is
# project-preauthorized and must be present before exporting the new XSA.
set_property -dict [list \
    CONFIG.PCW_EN_SDIO0 {1} \
    CONFIG.PCW_SD0_PERIPHERAL_ENABLE {1} \
    CONFIG.PCW_SD0_SD0_IO {MIO 40 .. 45} \
    CONFIG.PCW_SD0_GRP_CD_ENABLE {1} \
    CONFIG.PCW_SD0_GRP_CD_IO {MIO 47} \
    CONFIG.PCW_SDIO_PERIPHERAL_FREQMHZ {100} \
] $ps

set current_mi [get_property CONFIG.NUM_MI $interconnect]
if {$current_mi ni {5 6}} {
    error "Expected axi_interconnect_0 NUM_MI to be 5 or 6; found $current_mi"
}

set uart_cell [get_bd_cells -quiet pl_hmi_uart_0]
if {$uart_cell eq ""} {
    if {[get_bd_intf_ports -quiet PL_HMI_UART] ne ""} {
        error "PL_HMI_UART port exists without pl_hmi_uart_0; refusing partial integration"
    }

    set_property CONFIG.NUM_MI {6} $interconnect
    create_pl_hmi_uart_subsystem / pl_hmi_uart_0

    connect_bd_intf_net -intf_net pl_hmi_uart_axi \
        [get_bd_intf_pins axi_interconnect_0/M05_AXI] \
        [get_bd_intf_pins pl_hmi_uart_0/S_AXI]

    connect_bd_net [get_bd_pins processing_system7_0/FCLK_CLK0] \
        [get_bd_pins axi_interconnect_0/M05_ACLK] \
        [get_bd_pins pl_hmi_uart_0/s_axi_aclk]
    connect_bd_net [get_bd_pins proc_sys_reset_0/peripheral_aresetn] \
        [get_bd_pins axi_interconnect_0/M05_ARESETN] \
        [get_bd_pins pl_hmi_uart_0/s_axi_aresetn]

    set uart_port [create_bd_intf_port -mode Master \
        -vlnv xilinx.com:interface:uart_rtl:1.0 PL_HMI_UART]
    connect_bd_intf_net -intf_net pl_hmi_uart_external \
        $uart_port [get_bd_intf_pins pl_hmi_uart_0/UART]

    assign_bd_address -offset 0x43C30000 -range 0x00010000 \
        -target_address_space [get_bd_addr_spaces processing_system7_0/Data] \
        [get_bd_addr_segs pl_hmi_uart_0/axi_uartlite_core/S_AXI/Reg] -force
} else {
    if {$current_mi ne "6"} {
        error "pl_hmi_uart_0 exists but axi_interconnect_0 NUM_MI is $current_mi"
    }
    if {[get_bd_intf_ports -quiet PL_HMI_UART] eq ""} {
        error "pl_hmi_uart_0 exists without the PL_HMI_UART external interface"
    }
}

set uart_core [require_bd_cell pl_hmi_uart_0/axi_uartlite_core]
foreach {property expected} [list \
    CONFIG.C_BAUDRATE {115200} \
    CONFIG.C_DATA_BITS {8} \
    CONFIG.C_USE_PARITY {0} \
    CONFIG.C_S_AXI_ACLK_FREQ_HZ {100000000} \
] {
    set actual [get_property $property $uart_core]
    if {$actual ne $expected} {
        error "UART integration mismatch: $property expected '$expected' got '$actual'"
    }
}

foreach {property expected} [list \
    CONFIG.PCW_EN_SDIO0 {1} \
    CONFIG.PCW_SD0_PERIPHERAL_ENABLE {1} \
    CONFIG.PCW_SD0_SD0_IO {MIO 40 .. 45} \
    CONFIG.PCW_SD0_GRP_CD_ENABLE {1} \
    CONFIG.PCW_SD0_GRP_CD_IO {MIO 47} \
] {
    set actual [get_property $property $ps]
    if {$actual ne $expected} {
        error "SD0 integration mismatch: $property expected '$expected' got '$actual'"
    }
}

if {[get_files -quiet $board_xdc] eq ""} {
    error "J11 board constraint is not part of the active project: $board_xdc"
}

validate_bd_design
save_bd_design
generate_target all [get_files $bd_file] -force
make_wrapper -files [get_files $bd_file] -top -force
set wrapper_object [get_files -quiet $wrapper_file]
if {$wrapper_object ne ""} {
    # A relocated clean clone may auto-disable the missing generated wrapper
    # while opening the XPR. Remove and re-add the newly generated file so it
    # is active in synthesis instead of retaining that transient state.
    remove_files $wrapper_object
}
add_files -norecurse $wrapper_file
update_compile_order -fileset sources_1
write_bd_tcl -force $bd_tcl_file
close_project

puts "PL_HMI_UART_25G_INTEGRATION_PASSED"
