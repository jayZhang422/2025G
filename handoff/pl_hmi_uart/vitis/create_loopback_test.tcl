# Create an independent Vitis 2020.2 standalone UARTLite loopback test.
# This script builds a platform, BSP, and ELF only. It never invokes Bootgen.

proc require_env {name} {
    if {![info exists ::env($name)] || $::env($name) eq ""} {
        error "Required environment variable is not set: $name"
    }
    return [file normalize $::env($name)]
}

proc read_define {header_text name} {
    set pattern [format {(?m)^#define[ \t]+%s[ \t]+([^\r\n]+)} $name]
    if {![regexp $pattern $header_text -> value]} {
        error "Generated xparameters.h does not define $name"
    }
    return [string trim $value]
}

proc integer_define {header_text name} {
    set value [read_define $header_text $name]
    set normalized [string trimright $value UuLl]
    if {![string is integer -strict $normalized]} {
        error "Generated $name is not an integer value: $value"
    }
    return $normalized
}

set script_dir [file dirname [file normalize [info script]]]
set handoff_dir [file normalize [file join $script_dir ..]]
set source_file [file join $handoff_dir software board_test \
    pl_hmi_uart_loopback.c]
set workspace_dir [require_env PL_HMI_UART_VITIS_WORKSPACE_DIR]
set xsa_file [require_env PL_HMI_UART_XSA_FILE]
set platform_name pl_hmi_uart_loopback_platform
set domain_name standalone_domain
set app_name pl_hmi_uart_loopback

if {![file isfile $xsa_file]} {
    error "Candidate XSA does not exist: $xsa_file"
}
if {![file isfile $source_file]} {
    error "Loopback source does not exist: $source_file"
}

file mkdir $workspace_dir
setws $workspace_dir

if {[catch {platform active $platform_name}]} {
    platform create -name $platform_name -hw $xsa_file \
        -proc ps7_cortexa9_0 -os standalone
    platform active $platform_name
} else {
    platform config -updatehw $xsa_file
}

domain active $domain_name
bsp config stdin ps7_uart_1
bsp config stdout ps7_uart_1
platform generate

set xparameters [file join $workspace_dir $platform_name export \
    $platform_name sw $platform_name $domain_name bspinclude include \
    xparameters.h]
if {![file isfile $xparameters]} {
    error "Generated xparameters.h not found: $xparameters"
}

set handle [open $xparameters r]
set xparameters_text [read $handle]
close $handle

set instance_count [integer_define $xparameters_text \
    XPAR_XUARTLITE_NUM_INSTANCES]
set device_id [integer_define $xparameters_text XPAR_UARTLITE_0_DEVICE_ID]
set uart_base [read_define $xparameters_text XPAR_UARTLITE_0_BASEADDR]
set uart_high [read_define $xparameters_text XPAR_UARTLITE_0_HIGHADDR]
set baudrate [integer_define $xparameters_text XPAR_UARTLITE_0_BAUDRATE]
set parity [integer_define $xparameters_text XPAR_UARTLITE_0_USE_PARITY]
set data_bits [integer_define $xparameters_text XPAR_UARTLITE_0_DATA_BITS]
set stdin_base [read_define $xparameters_text STDIN_BASEADDRESS]
set stdout_base [read_define $xparameters_text STDOUT_BASEADDRESS]
set uart1_base [read_define $xparameters_text XPAR_PS7_UART_1_BASEADDR]

if {$instance_count != 1 || $baudrate != 115200 ||
    $parity != 0 || $data_bits != 8} {
    error "UARTLite BSP mismatch: count=$instance_count baud=$baudrate parity=$parity bits=$data_bits"
}
if {$stdin_base ne $uart1_base || $stdout_base ne $uart1_base} {
    error "Diagnostic console is not PS UART1: stdin=$stdin_base stdout=$stdout_base uart1=$uart1_base"
}

set config_source [file join $workspace_dir $platform_name \
    ps7_cortexa9_0 $domain_name bsp ps7_cortexa9_0 libsrc \
    uartlite_v3_5 src xuartlite_g.c]
if {![file isfile $config_source]} {
    error "Generated UARTLite configuration table not found: $config_source"
}
set handle [open $config_source r]
set config_text [read $handle]
close $handle
foreach macro [list \
    XPAR_UARTLITE_0_DEVICE_ID \
    XPAR_UARTLITE_0_BASEADDR \
    XPAR_UARTLITE_0_BAUDRATE \
    XPAR_UARTLITE_0_USE_PARITY \
    XPAR_UARTLITE_0_ODD_PARITY \
    XPAR_UARTLITE_0_DATA_BITS] {
    if {[string first $macro $config_text] < 0} {
        error "Generated XUartLite_ConfigTable does not use $macro"
    }
}

if {[catch {app list} app_names] ||
    [lsearch -exact [split $app_names "\n"] $app_name] < 0} {
    app create -name $app_name -platform $platform_name \
        -domain $domain_name -template {Empty Application}
}
importsources -name $app_name -path $source_file \
    -target-path src -soft-link
app config -name $app_name -set compiler-optimization \
    {Optimize more (-O2)}
app build -name $app_name

set elf_file [file join $workspace_dir $app_name Debug ${app_name}.elf]
if {![file isfile $elf_file]} {
    error "Loopback ELF was not generated: $elf_file"
}

puts "PL_HMI_UART_LOOPBACK_BUILD_PASSED"
puts "PL_HMI_UART_XPARAMETERS=$xparameters"
puts "PL_HMI_UART_CONFIG=$config_source"
puts "PL_HMI_UART_DEVICE=$device_id BASE=$uart_base HIGH=$uart_high"
puts "PL_HMI_UART_FORMAT=115200_8N1"
puts "PL_HMI_UART_CONSOLE=PS_UART1 BASE=$uart1_base"
puts "PL_HMI_UART_LOOPBACK_ELF=$elf_file"
