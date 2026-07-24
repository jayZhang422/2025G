# Regenerate Block Design products, create the HDL wrapper, and export the BD Tcl.
# Run: vivado -mode batch -source pl_update_bd.tcl
# Check names only: vivado -mode batch -source pl_update_bd.tcl -tclargs --check
# Validate the BD only: vivado -mode batch -source pl_update_bd.tcl -tclargs --validate-only
# Resolve an ambiguous project once: ... -tclargs --project <path-to-xpr>

proc require_file {path description} {
    if {![file isfile $path]} {
        error "$description not found: $path"
    }
}

proc progress {message} {
    puts "[clock format [clock seconds] -format {%Y-%m-%d %H:%M:%S}] PL BD: $message"
    flush stdout
}

set script_dir [file dirname [file normalize [info script]]]
source [file join $script_dir pl_automation_helpers.tcl]
pl_automation::init $script_dir
set project_file [pl_automation::project_file [file normalize [file join $script_dir ..]]]
set project_dir [file dirname $project_file]
set project_name [pl_automation::project_name $project_file]

progress "checking project files"
require_file $project_file "Vivado project"
set opened_by_script [pl_automation::open_target_project $project_file]
set bd_file [pl_automation::bd_file]
set bd_name [file rootname [file tail $bd_file]]
set wrapper_file [file join $project_dir "$project_name.gen" sources_1 bd $bd_name hdl "${bd_name}_wrapper.v"]
set bd_tcl_file [file join $project_dir "${bd_name}.tcl"]
if {[get_files -quiet $bd_file] eq ""} {
    pl_automation::close_if_opened $opened_by_script
    error "Block Design is not part of project $project_name: $bd_file"
}

if {[lsearch -exact $argv "--check"] >= 0} {
    progress "checking generated wrapper and exported BD Tcl"
    require_file $wrapper_file "Generated HDL wrapper"
    require_file $bd_tcl_file "Existing Block Design export"
    progress "CHECK PASSED: $project_name, $bd_name.bd, ${bd_name}_wrapper.v, and $bd_tcl_file paths are valid"
    pl_automation::close_if_opened $opened_by_script
    return
}

pl_automation::remember

progress "opening system.bd"
open_bd_design $bd_file
progress "validating Block Design (Vivado F6)"
if {[catch {validate_bd_design} validation_error]} {
    puts stderr "ERROR: Block Design validation failed. No project, wrapper, or export file was saved."
    puts stderr "Vivado details: $validation_error"
    pl_automation::close_if_opened $opened_by_script
    error "Stop: correct the Block Design validation errors and run this script again."
}
progress "Block Design validation passed"

if {[lsearch -exact $argv "--validate-only"] >= 0} {
    progress "VALIDATION PASSED: no files were generated or exported"
    pl_automation::close_if_opened $opened_by_script
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
pl_automation::close_if_opened $opened_by_script
progress "DONE: regenerated system BD products, added system_wrapper.v, and exported $bd_tcl_file"
