# Portable Vivado 2020.2 Block Design subsystem for the TJC HMI UART.
# The caller owns AXI address assignment and board-level UART constraints.

namespace eval pl_hmi_uart {
    variable version 1.0
    variable default_baud_rate 115200
    variable default_clock_hz 100000000
}

proc create_pl_hmi_uart_subsystem {
    parent_cell
    instance_name
    {baud_rate 115200}
    {clock_hz 100000000}
} {
    if {$parent_cell eq "" || $instance_name eq ""} {
        error "create_pl_hmi_uart_subsystem requires parent_cell and instance_name"
    }
    if {![string match "2020.2*" [version -short]]} {
        error "PL HMI UART requires Vivado 2020.2; found [version -short]"
    }
    if {$baud_rate <= 0 || $clock_hz <= 0} {
        error "PL HMI UART baud rate and AXI clock must be positive"
    }
    if {[get_ipdefs -all -quiet xilinx.com:ip:axi_uartlite:2.0] eq ""} {
        error "Xilinx AXI UARTLite 2.0 is not available in the IP catalog"
    }

    set parent_object [get_bd_cells $parent_cell]
    if {$parent_object eq ""} {
        error "PL HMI UART parent cell does not exist: $parent_cell"
    }
    if {[get_property TYPE $parent_object] ne "hier"} {
        error "PL HMI UART parent is not hierarchical: $parent_cell"
    }
    if {[get_bd_cells -quiet [format "%s/%s" $parent_cell $instance_name]] ne ""} {
        error "PL HMI UART instance already exists: $parent_cell/$instance_name"
    }

    set previous_instance [current_bd_instance .]
    current_bd_instance $parent_object
    set subsystem [create_bd_cell -type hier $instance_name]
    current_bd_instance $subsystem

    set s_axi [create_bd_intf_pin -mode Slave \
        -vlnv xilinx.com:interface:aximm_rtl:1.0 S_AXI]
    set uart [create_bd_intf_pin -mode Master \
        -vlnv xilinx.com:interface:uart_rtl:1.0 UART]
    set clock [create_bd_pin -dir I -type clk s_axi_aclk]
    set resetn [create_bd_pin -dir I -type rst s_axi_aresetn]
    set interrupt [create_bd_pin -dir O -type intr interrupt]

    set core [create_bd_cell -type ip \
        -vlnv xilinx.com:ip:axi_uartlite:2.0 axi_uartlite_core]
    set_property -dict [list \
        CONFIG.C_BAUDRATE $baud_rate \
        CONFIG.C_DATA_BITS {8} \
        CONFIG.C_S_AXI_ACLK_FREQ_HZ $clock_hz \
        CONFIG.C_USE_PARITY {0} \
    ] $core

    connect_bd_intf_net $s_axi [get_bd_intf_pins $core/S_AXI]
    connect_bd_intf_net $uart [get_bd_intf_pins $core/UART]
    connect_bd_net $clock [get_bd_pins $core/s_axi_aclk]
    connect_bd_net $resetn [get_bd_pins $core/s_axi_aresetn]
    connect_bd_net $interrupt [get_bd_pins $core/interrupt]

    foreach {property expected} [list \
        CONFIG.C_BAUDRATE $baud_rate \
        CONFIG.C_DATA_BITS {8} \
        CONFIG.C_USE_PARITY {0} \
        CONFIG.C_S_AXI_ACLK_FREQ_HZ $clock_hz \
    ] {
        set actual [get_property $property $core]
        if {$actual ne "$expected"} {
            error "PL HMI UART core mismatch: $property expected '$expected' got '$actual'"
        }
    }

    current_bd_instance $previous_instance
    return $subsystem
}
