namespace eval pl_automation {
    variable remembered [dict create]
    variable pending [dict create]
    variable script_dir ""
}

proc pl_automation::init {directory} {
    variable script_dir
    variable remembered
    set script_dir $directory
    set cache_file [file join $script_dir .automation_targets.tcl]
    if {[file isfile $cache_file]} {
        source $cache_file
    }
}

proc pl_automation::option {name} {
    variable remembered
    variable pending
    variable script_dir
    set index [lsearch -exact $::argv $name]
    if {$index >= 0} {
        if {$index + 1 >= [llength $::argv] || [string match --* [lindex $::argv [expr {$index + 1}]]]} {
            error "$name requires a value"
        }
        set value [lindex $::argv [expr {$index + 1}]]
        if {[file pathtype $value] ne "absolute"} { set value [file join $script_dir $value] }
        set value [file normalize $value]
        dict set pending $name $value
        return $value
    }
    if {[dict exists $remembered $name]} {
        return [dict get $remembered $name]
    }
    return ""
}

proc pl_automation::choice {name} {
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

proc pl_automation::unique_file {files description option_name} {
    set files [lsort -unique $files]
    if {[llength $files] != 1} {
        error "Could not uniquely identify $description. Run again with $option_name <path>; the choice will be remembered. Candidates: $files"
    }
    return [lindex $files 0]
}

proc pl_automation::project_file {project_root} {
    set selected [option --project]
    if {$selected ne ""} {
        if {![file isfile $selected]} { error "Vivado project not found: $selected" }
        return $selected
    }
    return [unique_file [glob -nocomplain -types f -directory $project_root *.xpr] "Vivado project" --project]
}

proc pl_automation::remember {} {
    variable pending
    variable remembered
    variable script_dir
    if {[dict size $pending] == 0} { return }
    dict for {key value} $pending { dict set remembered $key $value }
    set handle [open [file join $script_dir .automation_targets.tcl] w]
    puts $handle "# Generated from explicit automation-script arguments."
    puts $handle [list set ::pl_automation::remembered $remembered]
    close $handle
}

proc pl_automation::project_name {project_file} {
    return [file rootname [file tail $project_file]]
}

proc pl_automation::open_target_project {project_file} {
    set project_dir [file normalize [file dirname $project_file]]
    set current [current_project -quiet]
    if {$current eq ""} {
        open_project $project_file
        return 1
    }
    if {[file normalize [get_property DIRECTORY $current]] ne $project_dir} {
        error "Another project is open: [get_property DIRECTORY $current]\nClose it or open $project_file before running this script."
    }
    return 0
}

proc pl_automation::close_if_opened {opened} {
    if {$opened} { close_project }
}

proc pl_automation::bd_file {} {
    set selected [option --bd]
    if {$selected ne ""} {
        if {![file isfile $selected]} { error "Block Design not found: $selected" }
        return $selected
    }
    set project_dir [get_property DIRECTORY [current_project]]
    set bd_files [glob -nocomplain -types f [file join $project_dir *.srcs sources_1 bd * *.bd]]
    return [unique_file $bd_files "Block Design" --bd]
}
