# Generate the BSP and exported static libraries for an existing platform.
# Usage: xsct ps_generate_platform_libs.tcl --workspace <directory>

proc fail {message} {
    puts stderr "ERROR: $message"
    exit 1
}

set workspace ""
set argcnt [llength $::argv]
for {set i 0} {$i < $argcnt} {incr i} {
    set arg [lindex $::argv $i]
    if {$arg eq "--workspace"} {
        incr i
        if {$i >= $argcnt} { fail "--workspace requires a directory" }
        set workspace [file normalize [lindex $::argv $i]]
    } else {
        fail "unknown argument: $arg"
    }
}

if {$workspace eq ""} { fail "usage: --workspace <directory>" }
if {![file isdirectory $workspace]} { fail "workspace does not exist: $workspace" }

setws $workspace
platform active Identification_platform
puts "platform recovery: generating BSP libraries in $workspace"
platform generate
platform write
puts "platform recovery: BSP/library generation completed"
exit 0
