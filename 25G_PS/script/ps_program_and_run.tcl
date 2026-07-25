# Program the PL and start the optimized Debug ELF on Cortex-A9 core 0.
# Run from CLion/Windows: ps_program_and_run.bat
# Check files only: ps_program_and_run.bat --check
# Resolve an ambiguous artifact once with --bit, --xsa, --ps7-init, or --elf.

proc require_file {path description} {
    if {![file isfile $path]} {
        error "$description not found: $path"
    }
}

proc progress {message} {
    puts "[clock format [clock seconds] -format {%Y-%m-%d %H:%M:%S}] PS run: $message"
    flush stdout
}

proc require_optimized_makefile {build_dir} {
    set makefile [file join $build_dir src subdir.mk]
    require_file $makefile "Generated application makefile"
    set handle [open $makefile r]
    set contents [read $handle]
    close $handle
    if {[string first "-O2" $contents] < 0} {
        error "Expected -O2 in $makefile. Build the application after refreshing Vitis project settings."
    }
}

set script_dir [file dirname [file normalize [info script]]]
source [file join $script_dir ps_automation_helpers.tcl]
ps_automation::run {
ps_automation::init $script_dir [file rootname [file tail [info script]]]
set workspace [file normalize [file join $script_dir ..]]
set platform_name [ps_automation::platform_name $workspace]
set bit_file [ps_automation::platform_hardware_file $workspace $platform_name *.bit "Platform hardware bitstream" --bit]
set xsa_file [ps_automation::platform_hardware_file $workspace $platform_name *.xsa "Platform hardware XSA" --xsa]
set ps7_init_file [ps_automation::platform_hardware_file $workspace $platform_name ps7_init.tcl "Platform PS7 initialization script" --ps7-init]
set elf_file [ps_automation::application_elf $workspace 1]
set app_debug_dir [file dirname $elf_file]

progress "checking programming artifacts"
require_optimized_makefile $app_debug_dir
foreach {path description} [list \
    $bit_file "Application bitstream" \
    $xsa_file "Platform XSA" \
    $ps7_init_file "PS7 initialization script" \
    $elf_file "Optimized Debug ELF"] {
    require_file $path $description
}

if {[lsearch -exact $argv "--check"] >= 0} {
    progress "CHECK PASSED: $bit_file, $xsa_file, $ps7_init_file, and $elf_file are available"
    return
}

ps_automation::remember

progress "connecting to hardware server"
connect
progress "resetting system"
targets -set -nocase -filter {name =~ "APU*"}
rst -system
after 3000
progress "programming PL bitstream $bit_file"
targets -set -nocase -filter {name =~ "APU*"}
fpga -file $bit_file
progress "loading hardware description"
loadhw -hw $xsa_file -mem-ranges [list {0x40000000 0xbfffffff}] -regs
configparams force-mem-access 1
progress "running PS7 initialization"
source $ps7_init_file
ps7_init
ps7_post_config
targets -set -nocase -filter {name =~ "*A9*#0"}
progress "downloading optimized application ELF $elf_file"
dow $elf_file
configparams force-mem-access 0
progress "starting Cortex-A9 core 0"
con
}
