#include "ad_fifo_monitor_axi.h"

XStatus AD_FIFO_MONITOR_AXI_Reg_SelfTest(void *baseaddr_p)
{
    UINTPTR baseaddr = (UINTPTR)baseaddr_p;

    if (AD_FIFO_MONITOR_AXI_mReadReg(baseaddr,
            AD_FIFO_MONITOR_AXI_VERSION_OFFSET) !=
        AD_FIFO_MONITOR_AXI_VERSION) {
        return XST_FAILURE;
    }

    return XST_SUCCESS;
}
