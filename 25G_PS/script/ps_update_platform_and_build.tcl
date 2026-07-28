# Update the Vitis platform with the latest PL XSA, reset/regenerate BSP, then clean and build the system project.
# Run from CLion/Windows: ps_update_platform_and_build.bat
# Check names only: ps_update_platform_and_build.bat --check
# Resolve an ambiguous target once with --xsa, --platform, --system, --app, --elf, or --bsp.

proc require_file {path description} {
    if {![file isfile $path]} {
        error "$description not found: $path"
    }
}

proc sha256_file {path} {
    require_file $path "SHA-256 input"
    if {[catch {exec certutil.exe -hashfile [file nativename $path] SHA256 2>@1} output]} {
        error "Could not calculate SHA-256 for $path: $output"
    }
    if {![regexp -nocase {[0-9a-f]{64}} $output digest]} {
        error "Could not read SHA-256 from certutil output for $path: $output"
    }
    return [string toupper $digest]
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

set script_dir [file dirname [file normalize [info script]]]
source [file join $script_dir ps_automation_helpers.tcl]
ps_automation::run {
ps_automation::init $script_dir [file rootname [file tail [info script]]]
set workspace [file normalize [file join $script_dir ..]]
set project_root [file dirname $workspace]
set xsa_file [ps_automation::find_file $project_root --xsa [glob -nocomplain -types f -directory $project_root */*.xsa] "Latest PL XSA"]
set platform_name [ps_automation::platform_name $workspace]
set system_name [ps_automation::system_name $workspace]
set elf_file [ps_automation::application_elf $workspace 0]
set app_debug_dir [file dirname $elf_file]

progress "checking latest PL XSA"
require_file $xsa_file "Latest PL XSA"
progress "opening platform $platform_name in the isolated automation workspace"
ps_automation::activate_platform $workspace $platform_name

if {[lsearch -exact $argv "--check"] >= 0} {
    progress "CHECK PASSED: workspace, platform $platform_name, system $system_name, and $xsa_file are available"
    return
}

ps_automation::remember

progress "updating platform hardware specification from $xsa_file"
platform config -updatehw $xsa_file
# GUI 'Revert BSP Sources' maps to regenerating sources from the active BSP settings.
progress "regenerating BSP sources from active settings"
bsp regenerate
progress "cleaning stale platform products"
platform clean
progress "building regenerated platform products"
platform generate
set platform_dir [ps_automation::platform_directory $workspace $platform_name]
set platform_xsa_files [glob -nocomplain -types f [file join $platform_dir hw *.xsa]]
if {[llength $platform_xsa_files] != 1} {
    error "Expected exactly one generated Platform hardware XSA in $platform_dir/hw; found: $platform_xsa_files"
}
set platform_xsa [lindex $platform_xsa_files 0]
progress "verifying source and Platform hardware XSA SHA-256"
set source_xsa_sha256 [sha256_file $xsa_file]
set platform_xsa_sha256 [sha256_file $platform_xsa]
if {$source_xsa_sha256 ne $platform_xsa_sha256} {
    error "Platform hardware XSA hash mismatch: source $xsa_file = $source_xsa_sha256; generated $platform_xsa = $platform_xsa_sha256"
}
progress "verified XSA SHA-256 $source_xsa_sha256"
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
progress "building system package $system_name from the new ELF and platform"
set boot_file [ps_automation::build_system_package $workspace $platform_name $system_name $elf_file]
progress "DONE: updated $platform_name from $xsa_file, built optimized $elf_file, and generated $boot_file"
}
