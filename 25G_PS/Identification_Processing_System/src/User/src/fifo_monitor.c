/******************************************************************************
 * fifo_monitor.c
 *
 * Polling access to the PL ADC FIFO/AXIS diagnostic monitor.
 ******************************************************************************/

#include "../include/fifo_monitor.h"
#include "../config/hardware_config.h"

#include "ad_fifo_monitor_axi.h"
#include "xil_printf.h"
#include "xstatus.h"

#define FIFO_MONITOR_BASEADDR       APP_FIFO_MONITOR_BASEADDR
#define FIFO_MONITOR_TIMEOUT_POLLS 2000U

static u64 fifo_monitor_read64(u32 low_offset, u32 high_offset)
{
    u32 low = AD_FIFO_MONITOR_AXI_mReadReg(FIFO_MONITOR_BASEADDR,
                                            low_offset);
    u32 high = AD_FIFO_MONITOR_AXI_mReadReg(FIFO_MONITOR_BASEADDR,
                                             high_offset);

    return ((u64)high << 32) | (u64)low;
}

int fifo_monitor_init(void)
{
    u32 version = AD_FIFO_MONITOR_AXI_mReadReg(
        FIFO_MONITOR_BASEADDR, AD_FIFO_MONITOR_AXI_VERSION_OFFSET);

    if (version != APP_FIFO_MONITOR_PROTOCOL_VERSION) {
        xil_printf("[FIFO] WARN: monitor version 0x%08x, expected 0x%08x\r\n",
                   (unsigned int)version,
                   (unsigned int)APP_FIFO_MONITOR_PROTOCOL_VERSION);
        return XST_FAILURE;
    }
    return XST_SUCCESS;
}

int fifo_monitor_snapshot(fifo_monitor_snapshot_t *snapshot)
{
    u32 poll;
    u32 status = 0U;

    if (snapshot == NULL) {
        return XST_FAILURE;
    }

    AD_FIFO_MONITOR_AXI_mWriteReg(
        FIFO_MONITOR_BASEADDR, AD_FIFO_MONITOR_AXI_CONTROL_OFFSET,
        AD_FIFO_MONITOR_AXI_CONTROL_SNAPSHOT_MASK);

    for (poll = 0U; poll < FIFO_MONITOR_TIMEOUT_POLLS; ++poll) {
        status = AD_FIFO_MONITOR_AXI_mReadReg(
            FIFO_MONITOR_BASEADDR, AD_FIFO_MONITOR_AXI_STATUS_OFFSET);
        if ((status & AD_FIFO_MONITOR_AXI_STATUS_SNAPSHOT_BUSY_MASK) == 0U &&
            (status & AD_FIFO_MONITOR_AXI_STATUS_SNAPSHOT_VALID_MASK) != 0U) {
            break;
        }
    }
    if (poll == FIFO_MONITOR_TIMEOUT_POLLS) {
        return XST_TIMEOUT;
    }

    snapshot->status = status;
    snapshot->version = AD_FIFO_MONITOR_AXI_mReadReg(
        FIFO_MONITOR_BASEADDR, AD_FIFO_MONITOR_AXI_VERSION_OFFSET);
    snapshot->adc_sample_count = fifo_monitor_read64(
        AD_FIFO_MONITOR_AXI_ADC_SAMPLE_LO_OFFSET,
        AD_FIFO_MONITOR_AXI_ADC_SAMPLE_HI_OFFSET);
    snapshot->fifo_write_count = fifo_monitor_read64(
        AD_FIFO_MONITOR_AXI_FIFO_WRITE_LO_OFFSET,
        AD_FIFO_MONITOR_AXI_FIFO_WRITE_HI_OFFSET);
    snapshot->blocked_high_watermark_count = fifo_monitor_read64(
        AD_FIFO_MONITOR_AXI_BLOCKED_HIGH_LO_OFFSET,
        AD_FIFO_MONITOR_AXI_BLOCKED_HIGH_HI_OFFSET);
    snapshot->blocked_reset_count = fifo_monitor_read64(
        AD_FIFO_MONITOR_AXI_BLOCKED_RESET_LO_OFFSET,
        AD_FIFO_MONITOR_AXI_BLOCKED_RESET_HI_OFFSET);
    snapshot->axis_beat_count = fifo_monitor_read64(
        AD_FIFO_MONITOR_AXI_AXIS_BEAT_LO_OFFSET,
        AD_FIFO_MONITOR_AXI_AXIS_BEAT_HI_OFFSET);
    snapshot->frame_count = fifo_monitor_read64(
        AD_FIFO_MONITOR_AXI_FRAME_LO_OFFSET,
        AD_FIFO_MONITOR_AXI_FRAME_HI_OFFSET);
    snapshot->axis_stall_cycle_count = fifo_monitor_read64(
        AD_FIFO_MONITOR_AXI_AXIS_STALL_LO_OFFSET,
        AD_FIFO_MONITOR_AXI_AXIS_STALL_HI_OFFSET);
    snapshot->last_frame_timestamp = fifo_monitor_read64(
        AD_FIFO_MONITOR_AXI_LAST_FRAME_TIME_LO_OFFSET,
        AD_FIFO_MONITOR_AXI_LAST_FRAME_TIME_HI_OFFSET);

    return XST_SUCCESS;
}

int fifo_monitor_clear_sticky(void)
{
    u32 poll;

    AD_FIFO_MONITOR_AXI_mWriteReg(
        FIFO_MONITOR_BASEADDR, AD_FIFO_MONITOR_AXI_CONTROL_OFFSET,
        AD_FIFO_MONITOR_AXI_CONTROL_CLEAR_STICKY_MASK);

    for (poll = 0U; poll < FIFO_MONITOR_TIMEOUT_POLLS; ++poll) {
        u32 status = AD_FIFO_MONITOR_AXI_mReadReg(
            FIFO_MONITOR_BASEADDR, AD_FIFO_MONITOR_AXI_STATUS_OFFSET);

        if ((status & AD_FIFO_MONITOR_AXI_STATUS_CLEAR_BUSY_MASK) == 0U) {
            return XST_SUCCESS;
        }
    }
    return XST_TIMEOUT;
}

static void fifo_monitor_print_u64(const char *name, u64 value)
{
    xil_printf(" %s=0x%08x%08x", name,
               (unsigned int)(value >> 32), (unsigned int)value);
}

void fifo_monitor_print(const char *tag,
                        const fifo_monitor_snapshot_t *snapshot)
{
    if (snapshot == NULL) {
        return;
    }

    xil_printf("[FIFO] %s status=0x%08x version=0x%08x\r\n", tag,
               (unsigned int)snapshot->status,
               (unsigned int)snapshot->version);
    xil_printf("[FIFO] ADC/FIFO:");
    fifo_monitor_print_u64("valid", snapshot->adc_sample_count);
    fifo_monitor_print_u64("written", snapshot->fifo_write_count);
    fifo_monitor_print_u64("blocked_full",
                           snapshot->blocked_high_watermark_count);
    fifo_monitor_print_u64("blocked_reset", snapshot->blocked_reset_count);
    xil_printf("\r\n[FIFO] AXIS:");
    fifo_monitor_print_u64("beats", snapshot->axis_beat_count);
    fifo_monitor_print_u64("frames", snapshot->frame_count);
    fifo_monitor_print_u64("stall", snapshot->axis_stall_cycle_count);
    fifo_monitor_print_u64("last_tlast_time", snapshot->last_frame_timestamp);
    xil_printf("\r\n");
}
