#include "User/include/g26_measurement_app.h"

#include "FreeRTOS.h"
#include "task.h"
#include "xil_printf.h"
#include "xstatus.h"

int main(void)
{
    if (xTaskCreate(g26_measurement_task, "g26_measure",
                    configMINIMAL_STACK_SIZE * 8U, NULL,
                    tskIDLE_PRIORITY + 1U, NULL) != pdPASS) {
        xil_printf("[G26] FATAL: task create failed\r\n");
        return XST_FAILURE;
    }

    vTaskStartScheduler();
    return XST_FAILURE;
}
