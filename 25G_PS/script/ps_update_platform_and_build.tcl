# Update the Vitis platform with the latest PL XSA, reset/regenerate BSP, then clean and build the system project.
# Run: xsct ps_update_platform_and_build.tcl
# Check names only: xsct ps_update_platform_and_build.tcl --check

proc require_file {path description} {
    if {![file isfile $path]} {
        error "$description not found: $path"
    }
}

proc progress {message} {
    puts "[clock format [clock seconds] -format {%Y-%m-%d %H:%M:%S}] PS platform: $message"
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
set xsa_file [file normalize [file join $workspace .. 2023H_PL 2023H_pl top.xsa]]
set platform_name Signal_separation_platform
set system_name Signal_separation_app_system
set app_debug_dir [file join $workspace Signal_separation_app Debug]
set elf_file [file join $app_debug_dir Signal_separation_app.elf]

progress "checking latest PL XSA"
require_file $xsa_file "Latest PL XSA"
progress "opening workspace $workspace"
setws $workspace
progress "activating platform $platform_name"
platform active $platform_name

if {[lsearch -exact $argv "--check"] >= 0} {
    progress "CHECK PASSED: workspace, platform $platform_name, system $system_name, and $xsa_file are available"
    return
}

progress "updating platform hardware specification from $xsa_file"
platform config -updatehw $xsa_file
# GUI 'Revert BSP Sources' maps to regenerating sources from the active BSP settings.
progress "regenerating BSP sources from active settings"
bsp regenerate
progress "cleaning platform products"
platform clean
progress "building platform products"
platform generate
progress "cleaning system project $system_name"
sysproj clean -name $system_name
progress "building system project $system_name"
sysproj build -name $system_name
require_optimized_makefile $app_debug_dir
progress "building application Debug ELF with -O2"
build_application $app_debug_dir
require_file $elf_file "Debug ELF"
progress "DONE: updated $platform_name from $xsa_file and built $system_name plus optimized $elf_file"
