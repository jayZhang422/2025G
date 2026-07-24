# Run synthesis, implementation, bitstream generation, then export the XSA with the bitstream.
# Run: vivado -mode batch -source pl_build_bitstream.tcl
# Override the default parallelism: vivado -mode batch -source pl_build_bitstream.tcl -tclargs --threads 24
# Force a full rebuild without a prompt: -tclargs --rebuild
# Keep an up-to-date bitstream without a prompt: -tclargs --keep
# Check names only: vivado -mode batch -source pl_build_bitstream.tcl -tclargs --check
# Resolve an ambiguous project once: ... -tclargs --project <path-to-xpr>

proc require_file {path description} {
    if {![file isfile $path]} {
        error "$description not found: $path"
    }
}

proc progress {message} {
    puts "[clock format [clock seconds] -format {%Y-%m-%d %H:%M:%S}] PL build: $message"
    flush stdout
}

proc run_is_current {run expected_status} {
    return [expr {[get_property STATUS $run] eq $expected_status && \
                  [get_property NEEDS_REFRESH $run] eq "0"}]
}

proc ask_current_bitstream_action {} {
    while {1} {
        puts "Existing bitstream is up to date."
        puts -nonewline "Select 1: rebuild synthesis, implementation, and bitstream; 0: keep current files [1/0]: "
        flush stdout
        if {[gets stdin choice] < 0} {
            error "No input received. Use --rebuild to force a rebuild or --keep to retain current files."
        }

        switch -- [string trim $choice] {
            1 { return rebuild }
            0 { return keep }
            default { puts "Invalid selection. Enter 1 or 0." }
        }
    }
}

set script_dir [file dirname [file normalize [info script]]]
source [file join $script_dir pl_automation_helpers.tcl]
pl_automation::init $script_dir
set project_dir [file normalize [file join $script_dir ..]]
set project_file [pl_automation::project_file $project_dir]
set project_dir [file dirname $project_file]
set project_name [pl_automation::project_name $project_file]
set threads 24
set thread_arg [lsearch -exact $argv "--threads"]
if {$thread_arg >= 0} {
    if {$thread_arg + 1 >= [llength $argv]} {
        error "--threads requires a positive integer"
    }
    set threads [lindex $argv [expr {$thread_arg + 1}]]
}
if {![string is integer -strict $threads] || $threads < 1} {
    error "--threads must be a positive integer; received '$threads'"
}

set has_rebuild [expr {[lsearch -exact $argv "--rebuild"] >= 0}]
set has_keep [expr {[lsearch -exact $argv "--keep"] >= 0}]
if {$has_rebuild && $has_keep} {
    error "Use either --rebuild or --keep, not both."
}

progress "checking project files"
require_file $project_file "Vivado project"
set opened_by_script [pl_automation::open_target_project $project_file]
set top_name [get_property TOP [get_filesets sources_1]]
if {$top_name eq ""} {
    pl_automation::close_if_opened $opened_by_script
    error "The synthesis top is not set in $project_file"
}
set xsa_file [file join $project_dir "$top_name.xsa"]
set bit_file [file join $project_dir "$project_name.runs" impl_1 "$top_name.bit"]
if {[get_runs -quiet impl_1] eq ""} {
    pl_automation::close_if_opened $opened_by_script
    error "Implementation run impl_1 does not exist in project $project_name"
}
if {[get_runs -quiet synth_1] eq ""} {
    pl_automation::close_if_opened $opened_by_script
    error "Synthesis run synth_1 does not exist in project $project_name"
}

# Validate the requested concurrency against this Vivado version even in check mode.
set_param general.maxThreads $threads
if {[lsearch -exact $argv "--check"] >= 0} {
    progress "checking existing bitstream and hardware export"
    require_file $bit_file "Existing bitstream"
    require_file $xsa_file "Existing hardware export"
    progress "CHECK PASSED: $project_name and implementation run impl_1 are available"
    pl_automation::close_if_opened $opened_by_script
    return
}

pl_automation::remember

set impl_run [get_runs impl_1]
set synth_run [get_runs synth_1]
set bitstream_current [expr {[file isfile $bit_file] && \
    [run_is_current $impl_run "write_bitstream Complete!"] && \
    [run_is_current $synth_run "synth_design Complete!"]}]

if {$bitstream_current} {
    progress "existing top.bit, synth_1, and impl_1 are up to date"
    if {$has_rebuild} {
        set build_action rebuild
        progress "--rebuild selected; starting a full rebuild"
    } elseif {$has_keep} {
        set build_action keep
        progress "--keep selected; retaining current bitstream and XSA"
    } else {
        set build_action [ask_current_bitstream_action]
    }

    if {$build_action eq "keep"} {
        progress "keeping current top.bit and top.xsa; no files were regenerated"
        pl_automation::close_if_opened $opened_by_script
        return
    }
} else {
    progress "bitstream is missing or out of date; a full rebuild is required"
    if {$has_keep} {
        progress "--keep ignored because current hardware outputs are not valid"
    }
}

# 'general.maxThreads' caps parallel Vivado tool work. '-jobs' caps concurrent
# child runs. With one synth_1/impl_1 flow they are limits, not a guarantee of
# 24 busy CPUs. Reset both runs so this command always rebuilds all outputs.
progress "setting Vivado maximum worker threads to $threads and run-job limit to $threads"
progress "resetting implementation run impl_1"
reset_runs [get_runs impl_1]
progress "resetting synthesis run synth_1"
reset_runs [get_runs synth_1]
progress "launching synthesis run synth_1"
launch_runs synth_1 -jobs $threads
progress "waiting for synthesis completion"
wait_on_run synth_1
progress "launching implementation run impl_1 through write_bitstream"
launch_runs impl_1 -to_step write_bitstream -jobs $threads
progress "waiting for implementation and bitstream generation"
wait_on_run impl_1
progress "checking generated top.bit"
require_file $bit_file "Generated bitstream"

progress "exporting hardware platform with bitstream to top.xsa"
write_hw_platform -fixed -include_bit -force -file $xsa_file
pl_automation::close_if_opened $opened_by_script
progress "DONE: generated $bit_file and exported $xsa_file with the bitstream"
