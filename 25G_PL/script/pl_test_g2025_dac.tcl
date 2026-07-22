proc progress {message} { puts "[clock format [clock seconds] -format {%Y-%m-%d %H:%M:%S}] G2025 DAC TEST: $message"; flush stdout }
set script_dir [file dirname [file normalize [info script]]]
set project_dir [file normalize [file join $script_dir ..]]
set work_dir [file join $project_dir 25G_PL.sim g2025_dac_regression]
set dds_file [file join $project_dir ip_core DDS_DAC_ip DAC_DDS_Output.sv]
set adapter_file [file join $project_dir 25G_PL.srcs sources_1 new g2025_dac_adapter_v2.sv]
set testbench [file join $project_dir sim ip tb_g2025_dac_adapter_v2.sv]
foreach path [list $dds_file $adapter_file $testbench] { if {![file isfile $path]} { error "missing $path" } }
progress "creating isolated project"
create_project -force g2025_dac_regression $work_dir -part xc7z020clg400-2
set_property target_language Verilog [current_project]
set_property simulator_language Mixed [current_project]
add_files -norecurse [list $dds_file $adapter_file]
add_files -fileset sim_1 -norecurse $testbench
set_property top tb_g2025_dac_adapter_v2 [get_filesets sim_1]
update_compile_order -fileset sources_1
update_compile_order -fileset sim_1
set_property xsim.simulate.runtime {20 us} [get_filesets sim_1]
progress "launching simulation"
launch_simulation -simset sim_1 -mode behavioral
close_sim
close_project
progress "G2025 DAC ADAPTER REGRESSION PASSED"
