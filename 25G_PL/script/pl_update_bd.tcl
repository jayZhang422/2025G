# Regenerate Block Design products, create the HDL wrapper, and export the BD Tcl.
# Run: vivado -mode batch -source pl_update_bd.tcl
# Check names only: vivado -mode batch -source pl_update_bd.tcl -tclargs --check
# Validate the BD only: vivado -mode batch -source pl_update_bd.tcl -tclargs --validate-only

proc require_file {path description} {
    if {![file isfile $path]} {
        error "$description not found: $path"
    }
}

proc progress {message} {
    puts "[clock format [clock seconds] -format {%Y-%m-%d %H:%M:%S}] PL BD: $message"
    flush stdout
}

proc open_target_project {project_file project_dir} {
    set current [current_project -quiet]
    if {$current eq ""} {
        progress "target project is not open; opening $project_file"
        open_project $project_file
        return 1
    }

    set current_dir [file normalize [get_property DIRECTORY $current]]
    if {$current_dir ne $project_dir} {
        error "Another project is open: $current_dir\nClose it or open 25G_PL before running this script."
    }

    progress "using the already-open project 25G_PL"
    return 0
}

proc close_if_opened_by_script {opened_by_script} {
    if {$opened_by_script} {
        progress "closing project opened by this script"
        close_project
    }
}

set script_dir [file dirname [file normalize [info script]]]
set project_dir [file normalize [file join $script_dir ..]]
set project_file [file join $project_dir 25G_PL.xpr]
set bd_file [file join $project_dir 25G_PL.srcs sources_1 bd system system.bd]
set wrapper_file [file join $project_dir 25G_PL.srcs sources_1 imports system_wrapper.v]
set bd_tcl_file [file join $project_dir system.tcl]

progress "checking project files"
require_file $project_file "Vivado project"
require_file $bd_file "Block Design"

set opened_by_script [open_target_project $project_file $project_dir]
if {[get_files -quiet $bd_file] eq ""} {
    close_if_opened_by_script $opened_by_script
    error "Block Design is not part of project 25G_PL: $bd_file"
}

if {[lsearch -exact $argv "--check"] >= 0} {
    progress "checking generated wrapper and exported BD Tcl"
    require_file $wrapper_file "Generated HDL wrapper"
    require_file $bd_tcl_file "Existing Block Design export"
    progress "CHECK PASSED: 25G_PL, system.bd, system_wrapper.v, and system.tcl paths are valid"
    close_if_opened_by_script $opened_by_script
    return
}

progress "opening system.bd"
open_bd_design $bd_file
progress "validating Block Design (Vivado F6)"
if {[catch {validate_bd_design} validation_error]} {
    puts stderr "ERROR: Block Design validation failed. No project, wrapper, or export file was saved."
    puts stderr "Vivado details: $validation_error"
    close_if_opened_by_script $opened_by_script
    error "Stop: correct the Block Design validation errors and run this script again."
}
progress "Block Design validation passed"

if {[lsearch -exact $argv "--validate-only"] >= 0} {
    progress "VALIDATION PASSED: no files were generated or exported"
    close_if_opened_by_script $opened_by_script
    return
}

progress "saving validated Block Design"
save_bd_design
progress "regenerating Output Products for system.bd"
generate_target all [get_files $bd_file] -force
progress "regenerating HDL wrapper"
make_wrapper -files [get_files $bd_file] -top -force
require_file $wrapper_file "Generated HDL wrapper"

if {[get_files -quiet $wrapper_file] eq ""} {
    progress "adding generated system_wrapper.v to sources_1"
    add_files -norecurse $wrapper_file
}
progress "updating compile order"
update_compile_order -fileset sources_1

# This matches File -> Export -> Export Block Design.
progress "exporting Block Design Tcl to system.tcl"
write_bd_tcl -force $bd_tcl_file
close_if_opened_by_script $opened_by_script
progress "DONE: regenerated system BD products, added system_wrapper.v, and exported $bd_tcl_file"
