# Capture one 4096-sample ADC frame through the existing simple S2MM DMA.
# This bench script pauses Cortex-A9 core 0 while it owns the DMA registers,
# exports 4096 halfwords, then restores the CPU run state.

set dma_cr       0x40400030
set dma_sr       0x40400034
set dma_destaddr 0x40400048
set dma_btt      0x40400058
set capture_addr 0x01000000
set capture_size 0x00002000
set capture_marker 0x0000A5A5
set capture_file [file normalize [file join [file dirname [info script]] adc_capture.bin]]

connect
targets -set -nocase -filter {name =~ "*A9*#0"}
stop
configparams force-mem-access 1

set capture_status [catch {
    puts [format "DMA_BEFORE_CR=0x%08X DMA_BEFORE_SR=0x%08X" \
        [mrd -value $dma_cr] [mrd -value $dma_sr]]

    mwr $dma_cr 0x00000004
    set reset_ok 0
    for {set i 0} {$i < 100} {incr i} {
        after 10
        set cr_value [mrd -value $dma_cr]
        if {($cr_value & 0x4) == 0} {
            set reset_ok 1
            break
        }
    }
    if {!$reset_ok} {
        error "DMA reset timeout"
    }

    # Mark every halfword so a timeout can distinguish no AXIS traffic from
    # a nearly complete frame that is only missing TLAST.
    mwr -size h $capture_addr $capture_marker 4096
    mwr $dma_sr 0x00007000
    mwr $dma_destaddr $capture_addr
    mwr $dma_cr 0x00000001
    mwr $dma_btt $capture_size

    set capture_done 0
    set sr_value 0
    for {set i 0} {$i < 200} {incr i} {
        after 10
        set sr_value [mrd -value $dma_sr]
        if {($sr_value & 0x770) != 0} {
            error [format "DMA error SR=0x%08X" $sr_value]
        }
        if {($sr_value & 0x2) != 0} {
            set capture_done 1
            break
        }
    }
    if {!$capture_done} {
        puts [format "DMA_TIMEOUT_CR=0x%08X DMA_TIMEOUT_SR=0x%08X DMA_TIMEOUT_BTT=0x%08X" \
            [mrd -value $dma_cr] $sr_value [mrd -value $dma_btt]]
        mrd -bin -file $capture_file -size h $capture_addr 4096
        puts "ADC_TIMEOUT_CAPTURE_FILE=$capture_file"
        error [format "DMA capture timeout SR=0x%08X" $sr_value]
    }

    puts [format "DMA_AFTER_CR=0x%08X DMA_AFTER_SR=0x%08X" \
        [mrd -value $dma_cr] $sr_value]
    mrd -bin -file $capture_file -size h $capture_addr 4096
    puts "ADC_CAPTURE_FILE=$capture_file"
} capture_error]

configparams force-mem-access 0
con
disconnect

if {$capture_status != 0} {
    error $capture_error
}
