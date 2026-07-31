namespace eval ps_automation {
    variable remembered [dict create]
    variable pending [dict create]
    variable script_dir ""
    variable cache_file ""
}

proc ps_automation::init {directory profile} {
    variable script_dir
    variable cache_file
    set script_dir $directory
    set cache_file [file join $script_dir ".automation_targets.$profile.tcl"]
    if {[file isfile $cache_file]} { source $cache_file }
}

proc ps_automation::option {name} {
    variable remembered
    variable pending
    set index [lsearch -exact $::argv $name]
    if {$index >= 0} {
        if {$index + 1 >= [llength $::argv] || [string match --* [lindex $::argv [expr {$index + 1}]]]} { error "$name requires a value" }
        set value [lindex $::argv [expr {$index + 1}]]
        dict set pending $name $value
        return $value
    }
    if {[dict exists $remembered $name]} { return [dict get $remembered $name] }
    return ""
}

proc ps_automation::unique {values description option_name} {
    set values [lsort -unique $values]
    if {[llength $values] != 1} { error "Could not uniquely identify $description. Run again with $option_name <value>; the choice will be remembered. Candidates: $values" }
    return [lindex $values 0]
}

proc ps_automation::find_file {workspace name candidates description} {
    set selected [option $name]
    if {$selected ne ""} {
        if {[file pathtype $selected] ne "absolute"} { set selected [file join $workspace $selected] }
        set selected [file normalize $selected]
        if {![file isfile $selected]} { error "$description not found: $selected" }
        return $selected
    }
    return [unique $candidates $description $name]
}

proc ps_automation::project_name {directory} {
    set handle [open [file join $directory .project] r]
    set content [read $handle]
    close $handle
    if {![regexp {<name>([^<]+)</name>} $content -> name]} { error "Cannot read Eclipse project name from $directory/.project" }
    return $name
}

proc ps_automation::read_text {path} {
    set handle [open $path r]
    set content [read $handle]
    close $handle
    return $content
}

proc ps_automation::application_directory {workspace} {
    set selected [option --app]
    if {$selected ne ""} {
        if {[file pathtype $selected] ne "absolute"} { set selected [file join $workspace $selected] }
        set selected [file normalize $selected]
        if {![file isfile [file join $selected .cproject]]} { error "Vitis application project not found: $selected" }
        return $selected
    }

    set directories {}
    foreach cproject [glob -nocomplain -types f -directory $workspace */.cproject] {
        set directory [file dirname $cproject]
        set project_file [file join $directory .project]
        if {[file isfile $project_file] &&
            [string first {artifactExtension="elf"} [read_text $cproject]] >= 0 &&
            [string first {com.xilinx.sdx.sdk.core.SdkProjectNature} [read_text $project_file]] >= 0} {
            lappend directories $directory
        }
    }
    return [unique $directories "Vitis application project" --app]
}

proc ps_automation::application_elf {workspace require_existing} {
    set selected [option --elf]
    if {$selected ne ""} {
        if {[file pathtype $selected] ne "absolute"} { set selected [file join $workspace $selected] }
        set elf_file [file normalize $selected]
    } else {
        set app_dir [application_directory $workspace]
        set elf_file [file join $app_dir Debug "[project_name $app_dir].elf"]
    }
    if {$require_existing && ![file isfile $elf_file]} { error "Application ELF not found: $elf_file" }
    return $elf_file
}

proc ps_automation::platform_directory {workspace platform_name} {
    set directories {}
    foreach marker [glob -nocomplain -types f -directory $workspace */platform.spr] {
        set directory [file dirname $marker]
        if {[project_name $directory] eq $platform_name} { lappend directories $directory }
    }
    return [unique $directories "Vitis platform directory for $platform_name" --platform]
}

proc ps_automation::system_directory {workspace system_name} {
    set directories {}
    foreach marker [glob -nocomplain -types f -directory $workspace */*.sprj] {
        set directory [file dirname $marker]
        if {[project_name $directory] eq $system_name} { lappend directories $directory }
    }
    return [unique $directories "Vitis system directory for $system_name" --system]
}

proc ps_automation::platform_hardware_file {workspace platform_name pattern description option_name} {
    set platform_dir [platform_directory $workspace $platform_name]
    return [find_file $workspace $option_name \
        [glob -nocomplain -types f [file join $platform_dir hw $pattern]] $description]
}

proc ps_automation::activate_platform {workspace platform_name} {
    set platform_dir [platform_directory $workspace $platform_name]
    set automation_workspace [file join $workspace .Xil ps_automation_workspace]
    file mkdir $automation_workspace
    setws $automation_workspace
    platform read [file join $platform_dir platform.spr]
    platform active $platform_name
}

proc ps_automation::ensure_application_makefiles {workspace platform_name app_dir build_dir} {
    set makefile [file join $build_dir src subdir.mk]

    # Refresh the managed project even when an old makefile exists so newly
    # added source directories are discovered without editing Debug/*.mk.
    setws $workspace
    platform active $platform_name
    app build -name [project_name $app_dir]
    # Managed build may recreate Debug/_sdk while refreshing the source tree.
    ensure_bsp_link $workspace $platform_name $build_dir

    if {![file isfile $makefile]} {
        error "Vitis did not regenerate the application makefile: $makefile"
    }
}

proc ps_automation::ensure_bsp_link {workspace platform_name build_dir} {
    set link_path [file join $build_dir _sdk bsp]
    if {[llength [glob -nocomplain -types f [file join $link_path * include xil_types.h]]] == 1} { return }
    if {[file exists $link_path] || [file isdirectory $link_path]} {
        error "Existing BSP path is incomplete: $link_path"
    }

    set bsp_root [option --bsp]
    if {$bsp_root ne ""} {
        if {[file pathtype $bsp_root] ne "absolute"} { set bsp_root [file join $workspace $bsp_root] }
        set bsp_root [file normalize $bsp_root]
    } else {
        set platform_dir [platform_directory $workspace $platform_name]
        set headers [glob -nocomplain -types f [file join $platform_dir * * bsp * include xil_types.h]]
        set header [unique $headers "application BSP xil_types.h" --bsp]
        set bsp_root [file dirname [file dirname [file dirname $header]]]
    }

    file mkdir [file dirname $link_path]
    if {[catch {exec cmd.exe /d /c mklink /J [file nativename $link_path] [file nativename $bsp_root] 2>@1} details]} {
        error "Could not create BSP junction $link_path -> $bsp_root: $details"
    }
    if {[llength [glob -nocomplain -types f [file join $link_path * include xil_types.h]]] != 1} {
        error "BSP junction does not expose one processor include directory: $link_path"
    }
}

proc ps_automation::build_application {build_dir clean_first} {
    set library_dirs {}
    foreach xil_library [glob -nocomplain -types f [file join $build_dir _sdk bsp * lib libxil.a]] {
        set library_dir [file dirname $xil_library]
        if {[file isfile [file join $library_dir libfreertos.a]]} { lappend library_dirs $library_dir }
    }
    set library_dir [unique $library_dirs "application BSP library directory" --bsp]

    set objects_file [file join $build_dir objects.mk]
    if {![file isfile $objects_file]} { error "Generated application library list not found: $objects_file" }
    set objects_content [read_text $objects_file]
    if {![regexp -line {^LIBS[[:space:]]*:=[[:space:]]*(.+)$} $objects_content -> libraries]} {
        error "Could not read LIBS from $objects_file"
    }
    set libraries "-L[file nativename $library_dir] $libraries"

    if {$clean_first} {
        set failed [catch {exec make -C $build_dir clean 2>@1} output]
        puts $output
        if {$failed} { error $output }
    }
    set failed [catch {exec make -C $build_dir all "LIBS=$libraries" 2>@1} output]
    puts $output
    if {$failed} { error $output }
}

proc ps_automation::require_valid_fsbl {fsbl_file} {
    if {![file isfile $fsbl_file]} { error "Platform FSBL not found: $fsbl_file" }
    set failed [catch {exec arm-none-eabi-nm $fsbl_file 2>@1} symbols]
    if {$failed} { error "Could not inspect Platform FSBL $fsbl_file: $symbols" }
    foreach symbol {_vector_table main} {
        set pattern [format {^[[:xdigit:]]+[[:space:]]+[A-Za-z][[:space:]]+%s$} $symbol]
        if {![regexp -line $pattern $symbols]} {
            error "Platform FSBL is missing defined symbol $symbol: $fsbl_file"
        }
    }
}

proc ps_automation::run_serial_make {directory targets} {
    set failed [catch {exec make -j 1 -C $directory {*}$targets 2>@1} output]
    puts $output
    if {$failed} { error $output }
}

proc ps_automation::rebuild_platform_fsbl {platform_dir export_file} {
    set fsbl_dir [file join $platform_dir zynq_fsbl]
    set bsp_dir [file join $fsbl_dir zynq_fsbl_bsp]
    set built_file [file join $fsbl_dir fsbl.elf]

    file delete -force $built_file
    run_serial_make $bsp_dir [list clean seq_libs par_libs archive]
    run_serial_make $fsbl_dir [list all]
    require_valid_fsbl $built_file
    file copy -force $built_file $export_file
    require_valid_fsbl $export_file
}

proc ps_automation::build_system_package {workspace platform_name system_name elf_file} {
    set platform_dir [platform_directory $workspace $platform_name]
    set bit_files [glob -nocomplain -types f [file join $platform_dir hw *.bit]]
    set bit_file [unique $bit_files "platform hardware bitstream" --bit]
    set fsbl_files [glob -nocomplain -types f \
        [file join $platform_dir export $platform_name sw $platform_name boot fsbl.elf]]
    set fsbl_file [unique $fsbl_files "platform FSBL" --platform]
    if {[catch {require_valid_fsbl $fsbl_file} reason]} {
        puts "Platform FSBL is invalid; rebuilding it serially: $reason"
        rebuild_platform_fsbl $platform_dir $fsbl_file
    }
    require_valid_fsbl $fsbl_file

    set system_dir [system_directory $workspace $system_name]
    set configuration [file tail [file dirname $elf_file]]
    set build_dir [file join $system_dir $configuration]
    file mkdir $build_dir
    set bif_file [file join $build_dir system.bif]
    set sd_card_dir [file join $build_dir sd_card]
    file mkdir $sd_card_dir
    set handle [open $bif_file w]
    puts $handle "the_ROM_image:"
    puts $handle "{"
    puts $handle "    \[bootloader\] [string map {\\ /} $fsbl_file]"
    puts $handle "    [string map {\\ /} $bit_file]"
    puts $handle "    [string map {\\ /} $elf_file]"
    puts $handle "}"
    close $handle

    set boot_file [file join $sd_card_dir BOOT.BIN]
    set failed [catch {exec bootgen -arch zynq -image $bif_file -o $boot_file -w on 2>@1} output]
    puts $output
    if {$failed} { error $output }
    if {![file isfile $boot_file]} { error "System BOOT.BIN was not generated: $boot_file" }
    return $boot_file
}

proc ps_automation::run {body} {
    set failed [catch {uplevel 1 $body} message options]
    if {$failed == 2 && [dict exists $options -code] && [dict get $options -code] == 0} {
        set failed 0
    }
    if {[info exists ::env(PS_AUTOMATION_STATUS_FILE)]} {
        set handle [open $::env(PS_AUTOMATION_STATUS_FILE) w]
        puts $handle [expr {$failed ? "FAIL" : "PASS"}]
        close $handle
    }
    if {$failed} {
        puts stderr $message
        exit 1
    }
    exit 0
}

proc ps_automation::platform_name {workspace} {
    set selected [option --platform]
    if {$selected ne ""} { return $selected }
    set names {}
    foreach file [glob -nocomplain -types f -directory $workspace */platform.spr] { lappend names [project_name [file dirname $file]] }
    return [unique $names "Vitis platform" --platform]
}

proc ps_automation::system_name {workspace} {
    set selected [option --system]
    if {$selected ne ""} { return $selected }
    set names {}
    foreach file [glob -nocomplain -types f -directory $workspace */*.sprj] { lappend names [project_name [file dirname $file]] }
    return [unique $names "Vitis system project" --system]
}

proc ps_automation::remember {} {
    variable pending
    variable remembered
    variable cache_file
    if {[dict size $pending] == 0} { return }
    dict for {key value} $pending { dict set remembered $key $value }
    set handle [open $cache_file w]
    puts $handle "# Generated from explicit automation-script arguments."
    puts $handle [list set ::ps_automation::remembered $remembered]
    close $handle
}
