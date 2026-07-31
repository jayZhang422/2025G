#include "User/include/g26_measurement_app.h"
#include "User/screen/g26_hmi_task.h"

#include "FreeRTOS.h"
#include "task.h"
#include "xil_printf.h"
#include "xstatus.h"

int main(void)
{
    if (g26_measurement_app_init() != XST_SUCCESS) {
        xil_printf("[G26] FATAL: measurement IPC initialization failed\r\n");
        return XST_FAILURE;
    }
    if (xTaskCreate(g26_measurement_task, "g26_measure",
                    configMINIMAL_STACK_SIZE * 8U, NULL,
                    tskIDLE_PRIORITY + 1U, NULL) != pdPASS) {
        xil_printf("[G26] FATAL: measurement task create failed\r\n");
        return XST_FAILURE;
    }
    if (xTaskCreate(g26_hmi_task, "g26_hmi",
                    configMINIMAL_STACK_SIZE * 6U, NULL,
                    tskIDLE_PRIORITY + 2U, NULL) != pdPASS) {
        xil_printf("[HMI] FATAL: screen task create failed\r\n");
        return XST_FAILURE;
    }

    vTaskStartScheduler();
    return XST_FAILURE;
}
