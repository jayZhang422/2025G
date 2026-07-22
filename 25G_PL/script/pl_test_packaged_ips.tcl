# Compile and behaviorally verify the two packaged teammate IPs in isolation.
# Run: vivado -mode batch -source script/pl_test_packaged_ips.tcl

proc progress {message} {
    puts "[clock format [clock seconds] -format {%Y-%m-%d %H:%M:%S}] IP TEST: $message"
    flush stdout
}

proc require_file {path description} {
    if {![file isfile $path]} {
        error "$description not found: $path"
    }
}

set script_dir [file dirname [file normalize [info script]]]
set project_dir [file normalize [file join $script_dir ..]]
set work_dir [file join $project_dir 25G_PL.sim packaged_ip_regression]
set dds_file [file join $project_dir ip_core DDS_DAC_ip DAC_DDS_Output.sv]
set adc_dir [file join $project_dir ip_core ad_fifo_ip src]
set adc_wrapper [file join $adc_dir ad_fifo_wrapper.v]
set adc_sampler [file join $adc_dir ad9226.v]
set fifo_wrapper [file join $adc_dir fifo.v]
set fifo_xci [file join $adc_dir fifo_generator_0 fifo_generator_0.xci]
set testbench [file join $project_dir sim ip tb_packaged_ips.sv]

foreach {path description} [list \
    $dds_file "DDS packaged RTL" \
    $adc_wrapper "ADC packaged wrapper" \
    $adc_sampler "ADC sampler RTL" \
    $fifo_wrapper "FIFO wrapper RTL" \
    $fifo_xci "FIFO Generator XCI" \
    $testbench "packaged IP testbench"] {
    require_file $path $description
}

progress "creating isolated regression project"
create_project -force packaged_ip_regression $work_dir -part xc7z020clg400-2
set_property target_language Verilog [current_project]
set_property simulator_language Mixed [current_project]
set_property ip_repo_paths [file join $project_dir ip_core] [current_project]
update_ip_catalog

progress "adding packaged RTL and FIFO Generator"
add_files -norecurse [list $dds_file $adc_sampler $fifo_wrapper $adc_wrapper]
read_ip $fifo_xci
generate_target simulation [get_ips fifo_generator_0]
add_files -fileset sim_1 -norecurse $testbench
set_property top tb_packaged_ips [get_filesets sim_1]
update_compile_order -fileset sources_1
update_compile_order -fileset sim_1

progress "launching behavioral simulation"
set_property xsim.simulate.runtime {20 us} [get_filesets sim_1]
launch_simulation -simset sim_1 -mode behavioral
close_sim
close_project
progress "PACKAGED IP REGRESSION PASSED"
