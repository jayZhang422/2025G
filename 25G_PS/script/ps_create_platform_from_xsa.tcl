# Create a clean, relocatable Vitis 2020.2 platform from the active PL XSA.
#
# Usage:
#   xsct ps_create_platform_from_xsa.tcl --workspace D:/path/to/workspace
#   xsct ps_create_platform_from_xsa.tcl --workspace D:/path/to/workspace --check
#
# The workspace must be dedicated to this project. The script refuses to
# overwrite an existing platform directory.

proc usage {} {
    puts "Usage: xsct ps_create_platform_from_xsa.tcl --workspace <directory> ?--check?"
}

proc require_file {path description} {
    if {![file isfile $path]} {
        error "$description not found: $path"
    }
}

proc progress {message} {
    puts "[clock format [clock seconds] -format {%Y-%m-%d %H:%M:%S}] platform recovery: $message"
    flush stdout
}

set workspace ""
set check_only 0
set argument_index 0

while {$argument_index < [llength $argv]} {
    set argument [lindex $argv $argument_index]
    switch -- $argument {
        --workspace {
            incr argument_index
            if {$argument_index >= [llength $argv]} {
                usage
                error "--workspace requires a directory"
            }
            set workspace [file normalize [lindex $argv $argument_index]]
        }
        --check {
            set check_only 1
        }
        default {
            usage
            error "Unknown argument: $argument"
        }
    }
    incr argument_index
}

if {$workspace eq ""} {
    usage
    error "A dedicated --workspace directory is required"
}

set script_dir [file dirname [file normalize [info script]]]
set ps_root [file normalize [file join $script_dir ..]]
set repo_root [file normalize [file join $ps_root ..]]
set xsa_file [file join $repo_root 25G_PL top.xsa]
set platform_name Identification_platform
set platform_dir [file join $workspace $platform_name]

require_file $xsa_file "Active PL XSA"

progress "checking XSA $xsa_file"
set hardware_design [hsi open_hw_design $xsa_file]
set processors [hsi get_cells -filter {IP_TYPE==PROCESSOR}]
if {[lsearch -exact $processors ps7_cortexa9_0] < 0} {
    hsi close_hw_design $hardware_design
    error "Expected ps7_cortexa9_0 in XSA; found: $processors"
}
hsi close_hw_design $hardware_design

if {$check_only} {
    progress "CHECK PASSED: XSA exposes ps7_cortexa9_0; target workspace is $workspace"
    return
}

if {[file exists $platform_dir]} {
    error "Refusing to overwrite existing platform directory: $platform_dir"
}

file mkdir $workspace
setws $workspace
progress "creating $platform_name in $workspace"
platform create -name $platform_name \
    -hw $xsa_file \
    -proc ps7_cortexa9_0 \
    -os freertos10_xilinx \
    -fsbl-target ps7_cortexa9_0 \
    -out $workspace
platform write
platform generate -domains
platform active $platform_name

set platforms [platform list]
if {[lsearch -exact $platforms $platform_name] < 0} {
    error "Platform creation returned but $platform_name is not listed: $platforms"
}

progress "DONE: active platform $platform_name uses $xsa_file"
