#include "signal_separator_task.h"

#include "signal_separator_app.h"

#include "xil_printf.h"
#include "xstatus.h"

/** FreeRTOS 应用任务：运行信号分离应用，并在应用异常退出时自删除。 */
void signal_separator_task(void *parameters)
{
    (void)parameters;

    if (signal_separator_run() != XST_SUCCESS) {
        xil_printf("[APP] ERROR: signal separator task stopped\r\n");
    }
    vTaskDelete(NULL);
}
