/*
    Copyright (C) 2017 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
    Copyright (c) 2012 - 2020 Xilinx, Inc. All Rights Reserved.
	SPDX-License-Identifier: MIT


    http://www.FreeRTOS.org
    http://aws.amazon.com/freertos


    1 tab == 4 spaces!
*/

#include "FreeRTOS.h"
#include "task.h"

#include "signal_separator_task.h"
#include "xil_printf.h"

#define APP_TASK_STACK_WORDS 2048U
#define APP_TASK_PRIORITY    (tskIDLE_PRIORITY + 2U)

/** 创建信号分离 FreeRTOS 任务并启动调度器；调度器返回视为异常。 */
int main(void)
{
    if (xTaskCreate(signal_separator_task, "signal_separator",
                    APP_TASK_STACK_WORDS, NULL, APP_TASK_PRIORITY,
                    NULL) != pdPASS) {
        xil_printf("ERROR: unable to create signal separator task\r\n");
        for (;;) {
        }
    }

    vTaskStartScheduler();
    for (;;) {
    }
}
