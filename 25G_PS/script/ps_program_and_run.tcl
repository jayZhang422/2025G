# Program the PL and start the optimized Debug ELF on Cortex-A9 core 0.
# Run: xsct ps_program_and_run.tcl
# Check files only: xsct ps_program_and_run.tcl --check

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
set workspace [file normalize [file join $script_dir ..]]
set app_debug_dir [file join $workspace Signal_separation_app Debug]
set bit_file [file join $workspace Signal_separation_app _ide bitstream top.bit]
set xsa_file [file join $workspace Signal_separation_platform export Signal_separation_platform hw top.xsa]
set ps7_init_file [file join $workspace Signal_separation_app _ide psinit ps7_init.tcl]
set elf_file [file join $workspace Signal_separation_app Debug Signal_separation_app.elf]

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
    progress "CHECK PASSED: top.bit, top.xsa, ps7_init.tcl, and Signal_separation_app.elf are available"
    return
}

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
