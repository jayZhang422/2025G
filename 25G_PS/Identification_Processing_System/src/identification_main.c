#include "User/include/button_input.h"
#include "User/include/ddc_validation.h"

#include "FreeRTOS.h"
#include "task.h"
#include "xil_io.h"
#include "xil_printf.h"
#include "xparameters_ps.h"
#include "xstatus.h"

#define APP_SLCR_LOCK_OFFSET           0x00000004U
#define APP_SLCR_UNLOCK_OFFSET         0x00000008U
#define APP_SLCR_FPGA_RST_CTRL_OFFSET  0x00000240U
#define APP_SLCR_LOCK_KEY              0x0000767BU
#define APP_SLCR_UNLOCK_KEY            0x0000DF0DU
#define APP_FCLK0_RESET_MASK           0x00000001U
#define APP_DDC_TASK_STACK_WORDS       (configMINIMAL_STACK_SIZE * 8U)

static void reset_pl_peripherals(void)
{
    u32 reset_state;

    Xil_Out32(XPS_SYS_CTRL_BASEADDR + APP_SLCR_UNLOCK_OFFSET,
              APP_SLCR_UNLOCK_KEY);
    reset_state = Xil_In32(XPS_SYS_CTRL_BASEADDR +
                           APP_SLCR_FPGA_RST_CTRL_OFFSET);
    Xil_Out32(XPS_SYS_CTRL_BASEADDR + APP_SLCR_FPGA_RST_CTRL_OFFSET,
              reset_state | APP_FCLK0_RESET_MASK);
    vTaskDelay(pdMS_TO_TICKS(20U));
    Xil_Out32(XPS_SYS_CTRL_BASEADDR + APP_SLCR_FPGA_RST_CTRL_OFFSET,
              reset_state);
    Xil_Out32(XPS_SYS_CTRL_BASEADDR + APP_SLCR_LOCK_OFFSET,
              APP_SLCR_LOCK_KEY);
    vTaskDelay(pdMS_TO_TICKS(20U));
}

static void ddc_validation_task(void *argument)
{
    button_input_t buttons;
    int measurement_complete = 0;

    (void)argument;
    portTASK_USES_FLOATING_POINT();
    if (button_input_init(&buttons) != XST_SUCCESS) {
        xil_printf("[DDC VALIDATE] FAIL: button init\r\n");
        vTaskDelete(NULL);
        return;
    }

    xil_printf("[DDC VALIDATE] ready: KEY1=start, KEY2=clear\r\n");
    for (;;) {
        if (!measurement_complete &&
            button_input_take_start_press(&buttons)) {
            (void)ddc_validation_run(&g_ddc_validation_config);
            measurement_complete = 1;
            xil_printf("[DDC VALIDATE] press KEY2 to clear\r\n");
        } else if (measurement_complete &&
                   button_input_take_reset_press(&buttons)) {
            reset_pl_peripherals();
            measurement_complete = 0;
            xil_printf("[DDC VALIDATE] cleared: press KEY1 to start\r\n");
        }
        vTaskDelay(1U);
    }
}

int main(void)
{
    if (xTaskCreate(ddc_validation_task, "ddc_validate",
                    APP_DDC_TASK_STACK_WORDS, NULL,
                    tskIDLE_PRIORITY + 1U, NULL) != pdPASS) {
        xil_printf("[DDC VALIDATE] FAIL: task create\r\n");
        return XST_FAILURE;
    }

    vTaskStartScheduler();
    return XST_FAILURE;
}
