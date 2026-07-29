# Run with Vivado 2020.2:
# vivado -mode batch -source validate_pl_hmi_uart_bd.tcl

set script_dir [file dirname [file normalize [info script]]]
set uart_script [file normalize [file join $script_dir .. pl_hmi_uart_bd.tcl]]
set original_dir [pwd]
set work_dir [file normalize [file join $::env(TEMP) \
    "pl_hmi_uart_validation_[pid]"]]

if {![file isfile $uart_script]} {
    error "UART subsystem script not found: $uart_script"
}

file mkdir $work_dir
cd $work_dir
create_project -in_memory pl_hmi_uart_validation -part xc7z020clg400-2
create_bd_design pl_hmi_uart_validation
source $uart_script
create_pl_hmi_uart_subsystem / pl_hmi_uart_0

make_bd_intf_pins_external [get_bd_intf_pins pl_hmi_uart_0/S_AXI]
make_bd_intf_pins_external [get_bd_intf_pins pl_hmi_uart_0/UART]
make_bd_pins_external [get_bd_pins pl_hmi_uart_0/s_axi_aclk]
make_bd_pins_external [get_bd_pins pl_hmi_uart_0/s_axi_aresetn]
make_bd_pins_external [get_bd_pins pl_hmi_uart_0/interrupt]

assign_bd_address -offset 0x40000000 -range 0x00010000 \
    -target_address_space [get_bd_addr_spaces S_AXI_0] \
    [get_bd_addr_segs pl_hmi_uart_0/axi_uartlite_core/S_AXI/Reg] -force
validate_bd_design

set core [get_bd_cells pl_hmi_uart_0/axi_uartlite_core]
foreach {property expected} [list \
    CONFIG.C_BAUDRATE {115200} \
    CONFIG.C_DATA_BITS {8} \
    CONFIG.C_USE_PARITY {0} \
    CONFIG.C_S_AXI_ACLK_FREQ_HZ {100000000} \
] {
    set actual [get_property $property $core]
    if {$actual ne $expected} {
        error "Validation mismatch: $property expected '$expected' got '$actual'"
    }
}

puts "PL_HMI_UART_BD_VALIDATION_PASSED"
close_project
cd $original_dir
if {[catch {file delete -force $work_dir} cleanup_error]} {
    puts "INFO: temporary validation directory retained: $work_dir"
}
