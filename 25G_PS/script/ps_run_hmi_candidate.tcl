# Program and run one explicitly supplied J11 HMI candidate without an FSBL.
# This script never invokes Bootgen and never creates BOOT.BIN.

proc require_env_file {name description} {
    if {![info exists ::env($name)] || $::env($name) eq ""} {
        error "Set $name to the $description"
    }
    set path [file normalize $::env($name)]
    if {![file isfile $path]} {
        error "$description not found: $path"
    }
    return $path
}

set bit_file [require_env_file PS_HMI_CANDIDATE_BIT "candidate bitstream"]
set xsa_file [require_env_file PS_HMI_CANDIDATE_XSA "candidate XSA"]
set ps7_init_file [require_env_file PS_HMI_CANDIDATE_PS7_INIT "matching ps7_init.tcl"]
set elf_file [require_env_file PS_HMI_CANDIDATE_ELF "candidate application ELF"]

puts "PS_HMI_RUN_BIT=$bit_file"
puts "PS_HMI_RUN_XSA=$xsa_file"
puts "PS_HMI_RUN_PS7_INIT=$ps7_init_file"
puts "PS_HMI_RUN_ELF=$elf_file"

connect
targets -set -nocase -filter {name =~ "APU*"}
rst -system
after 3000
targets -set -nocase -filter {name =~ "APU*"}
fpga -file $bit_file
loadhw -hw $xsa_file -mem-ranges [list {0x40000000 0xbfffffff}] -regs
configparams force-mem-access 1
source $ps7_init_file
ps7_init
ps7_post_config
targets -set -nocase -filter {name =~ "*A9*#0"}
dow $elf_file
configparams force-mem-access 0
con

puts "PS_HMI_CANDIDATE_RUN_STARTED"
