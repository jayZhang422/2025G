# Clean and build the existing Vitis system project after PS source changes.
# Run: xsct ps_rebuild_system.tcl
# Check names only: xsct ps_rebuild_system.tcl --check

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

proc build_application {build_dir} {
    puts [exec make -C $build_dir all 2>@1]
}

set script_dir [file dirname [file normalize [info script]]]
set workspace [file normalize [file join $script_dir ..]]
set system_name Signal_separation_app_system
set app_debug_dir [file join $workspace Signal_separation_app Debug]
set elf_file [file join $app_debug_dir Signal_separation_app.elf]

progress "opening workspace $workspace"
setws $workspace
if {[lsearch -exact $argv "--check"] >= 0} {
    progress "CHECK PASSED: workspace and system project $system_name are available"
    return
}

progress "cleaning system project $system_name"
sysproj clean -name $system_name
progress "building system project $system_name"
sysproj build -name $system_name
require_optimized_makefile $app_debug_dir
progress "building application Debug ELF with -O2"
build_application $app_debug_dir
require_file $elf_file "Debug ELF"
progress "DONE: cleaned and built $system_name plus optimized $elf_file"
