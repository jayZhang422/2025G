proc require_file {path description} { if {![file isfile $path]} { error "$description not found: $path" } }
set script_dir [file dirname [file normalize [info script]]]
set project_dir [file normalize [file join $script_dir ..]]
set project_file [file join $project_dir 25G_PL.xpr]
set legacy_wrapper [file join $project_dir 25G_PL.srcs sources_1 imports system_wrapper.v]
set generated_wrapper [file join $project_dir 25G_PL.gen sources_1 bd system hdl system_wrapper.v]
require_file $project_file "Vivado project"
require_file $generated_wrapper "Generated system wrapper"
open_project $project_file
set legacy_file [get_files -quiet $legacy_wrapper]
if {$legacy_file ne ""} { remove_files $legacy_file }
if {[get_files -quiet $generated_wrapper] eq ""} { add_files -norecurse $generated_wrapper }
update_compile_order -fileset sources_1
save_project_as -force 25G_PL $project_dir
close_project
puts "GENERATED_WRAPPER_SELECTED"
