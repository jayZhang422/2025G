# Build the current FreeRTOS application against a supplied J11 HMI XSA.
# This creates an isolated workspace and never invokes Bootgen.

proc require_file {path description} {
    if {![file isfile $path]} {
        error "$description not found: $path"
    }
}

proc read_file {path} {
    set handle [open $path r]
    set contents [read $handle]
    close $handle
    return $contents
}

if {![info exists ::env(PS_HMI_CANDIDATE_XSA)] ||
    $::env(PS_HMI_CANDIDATE_XSA) eq ""} {
    error "Set PS_HMI_CANDIDATE_XSA to the implemented XSA"
}
if {![info exists ::env(PS_HMI_CANDIDATE_WORKSPACE)] ||
    $::env(PS_HMI_CANDIDATE_WORKSPACE) eq ""} {
    error "Set PS_HMI_CANDIDATE_WORKSPACE to a new output directory"
}

set script_dir [file dirname [file normalize [info script]]]
set ps_dir [file normalize [file join $script_dir ..]]
set source_root [file join $ps_dir Identification_Processing_System src]
set xsa_file [file normalize $::env(PS_HMI_CANDIDATE_XSA)]
set workspace [file normalize $::env(PS_HMI_CANDIDATE_WORKSPACE)]
set platform_name hmi_candidate_platform
set domain_name freertos10_xilinx_domain
set app_name hmi_candidate_app

require_file $xsa_file "Implemented HMI XSA"
require_file [file join $source_root identification_main.c] "Application entry point"
set ps_prefix [string tolower [file normalize [file join $ps_dir .]]]
set workspace_prefix [string tolower [file normalize [file join $workspace .]]]
if {$workspace_prefix eq $ps_prefix ||
    [string first "${ps_prefix}/" "${workspace_prefix}/"] == 0} {
    error "Candidate workspace must be outside the source project: $workspace"
}
if {[file exists $workspace]} {
    error "Candidate workspace already exists; choose a new path: $workspace"
}

file mkdir $workspace
setws $workspace
platform create -name $platform_name -hw $xsa_file \
    -proc ps7_cortexa9_0 -os freertos10_xilinx -out $workspace
platform active $platform_name
domain active $domain_name
bsp config stdin ps7_uart_1
bsp config stdout ps7_uart_1
platform generate

app create -name $app_name -platform $platform_name \
    -domain $domain_name -template {Empty Application}
set app_source_dir [file join $workspace $app_name src]
file mkdir $app_source_dir
foreach source_item [glob -nocomplain -directory $source_root *] {
    file copy -force $source_item $app_source_dir
}

foreach include_dir [list \
    $app_source_dir \
    [file join $app_source_dir User include] \
    [file join $app_source_dir User config] \
    [file join $app_source_dir cmsis Include] \
] {
    app config -name $app_name -add include-path $include_dir
}
app config -name $app_name -add libraries m
app config -name $app_name -set compiler-optimization {Optimize more (-O2)}
app build -name $app_name

set elf_file [file join $workspace $app_name Debug ${app_name}.elf]
require_file $elf_file "HMI candidate ELF"

set xparameter_matches [glob -nocomplain -types f \
    [file join $workspace $platform_name * * bsp * include xparameters.h] \
    [file join $workspace $platform_name export $platform_name sw * \
        bspinclude include xparameters.h]]
set xparameter_matches [lsort -unique $xparameter_matches]
if {[llength $xparameter_matches] != 1} {
    error "Expected one generated xparameters.h, found: $xparameter_matches"
}
set xparameters [lindex $xparameter_matches 0]
set parameter_text [read_file $xparameters]

foreach {pattern description} [list \
    {#define XPAR_XUARTLITE_NUM_INSTANCES 1U?} "one UARTLite instance" \
    {#define XPAR_UARTLITE_0_DEVICE_ID 0U?} "canonical UARTLite device ID" \
    {#define XPAR_UARTLITE_0_BASEADDR 0x43C30000U?} "UARTLite base address" \
    {#define XPAR_UARTLITE_0_BAUDRATE 115200U?} "UARTLite baud rate" \
    {#define XPAR_UARTLITE_0_DATA_BITS 8U?} "UARTLite data bits" \
    {#define XPAR_UARTLITE_0_USE_PARITY 0U?} "UARTLite parity setting" \
] {
    if {![regexp $pattern $parameter_text]} {
        error "Generated BSP does not expose $description in $xparameters"
    }
}
if {![regexp {#define XPAR_PS7_UART_1_BASEADDR ([^\r\n]+)} \
          $parameter_text -> uart1_base] ||
    ![regexp {#define STDOUT_BASEADDRESS ([^\r\n]+)} \
          $parameter_text -> stdout_base] ||
    $uart1_base ne $stdout_base} {
    error "Generated BSP stdout is not PS UART1"
}

puts "PS_HMI_CANDIDATE_BUILD_PASSED"
puts "PS_HMI_CANDIDATE_XPARAMETERS=$xparameters"
puts "PS_HMI_CANDIDATE_ELF=$elf_file"
