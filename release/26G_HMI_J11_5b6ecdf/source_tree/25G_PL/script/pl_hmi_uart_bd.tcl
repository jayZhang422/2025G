# Compatibility entrypoint. The portable source now lives under ip_core,
# matching the teammate project layout.

set script_dir [file dirname [file normalize [info script]]]
set portable_source [file normalize [file join \
    $script_dir .. ip_core pl_hmi_uart src pl_hmi_uart_bd.tcl]]
if {![file isfile $portable_source]} {
    error "Portable J11 HMI UART source not found: $portable_source"
}
source $portable_source
