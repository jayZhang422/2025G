# Clean and build the existing Vitis system project after PS source changes.
# Run from CLion/Windows: ps_rebuild_system.bat
# Check names only: ps_rebuild_system.bat --check
# Resolve an ambiguous target once with --system, --app, --elf, or --bsp.

proc require_file {path description} {
    if {![file isfile $path]} {
        error "$description not found: $path"
    }
}

proc progress {message} {
    puts "[clock format [clock seconds] -format {%Y-%m-%d %H:%M:%S}] PS system: $message"
    flush stdout
}

proc require_optimized_makefile {build_dir} {
    set makefile [file join $build_dir src subdir.mk]
    require_file $makefile "Generated application makefile"
    set handle [open $makefile r]
    set contents [read $handle]
    close $handle
    if {[string first "-O2" $contents] < 0} {
        error "Expected -O2 in $makefile. Refresh the Vitis project so the Debug makefiles are regenerated from .cproject."
    }
}

set script_dir [file dirname [file normalize [info script]]]
source [file join $script_dir ps_automation_helpers.tcl]
ps_automation::run {
ps_automation::init $script_dir [file rootname [file tail [info script]]]
set workspace [file normalize [file join $script_dir ..]]
set platform_name [ps_automation::platform_name $workspace]
set system_name [ps_automation::system_name $workspace]
set elf_file [ps_automation::application_elf $workspace 0]
set app_debug_dir [file dirname $elf_file]

if {[lsearch -exact $argv "--check"] >= 0} {
    progress "CHECK PASSED: workspace and system project $system_name are available"
    return
}

ps_automation::remember

progress "checking application BSP link"
ps_automation::ensure_bsp_link $workspace $platform_name $app_debug_dir
if {![file isfile [file join $app_debug_dir src subdir.mk]]} {
    progress "regenerating application makefiles removed by Vitis Clean"
}
ps_automation::ensure_application_makefiles $workspace $platform_name [ps_automation::application_directory $workspace] $app_debug_dir
require_optimized_makefile $app_debug_dir
progress "cleaning and building application Debug ELF with -O2"
ps_automation::build_application $app_debug_dir 1
require_file $elf_file "Debug ELF"
progress "building system package $system_name from the new ELF"
set boot_file [ps_automation::build_system_package $workspace $platform_name $system_name $elf_file]
progress "DONE: built optimized $elf_file and generated $boot_file"
}
