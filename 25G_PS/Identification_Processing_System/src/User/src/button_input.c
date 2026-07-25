/******************************************************************************
 * button_input.c
 *
 * Each API consumes one active-low press after debounce and release.
 ******************************************************************************/

#include "../include/app_config.h"
#include "../include/button_input.h"

#include "FreeRTOS.h"
#include "task.h"

/** 将微秒级请求换算为至少一个 FreeRTOS tick 的任务延时。 */
static void button_input_delay_us(u32 delay_us)
{
    TickType_t ticks = pdMS_TO_TICKS((delay_us + 999U) / 1000U);

    vTaskDelay((ticks == 0U) ? 1U : ticks);
}

/** 对指定低有效 GPIO 去抖、等待释放，并消费一次完整按压事件。 */
static int button_input_take_press(button_input_t *buttons, u32 pin)
{
    if (XGpioPs_ReadPin(&buttons->instance, pin) !=
        APP_BUTTON_ACTIVE_LEVEL) {
        return 0;
    }

    button_input_delay_us(APP_BUTTON_DEBOUNCE_US);
    if (XGpioPs_ReadPin(&buttons->instance, pin) !=
        APP_BUTTON_ACTIVE_LEVEL) {
        return 0;
    }

    while (XGpioPs_ReadPin(&buttons->instance, pin) ==
           APP_BUTTON_ACTIVE_LEVEL) {
        button_input_delay_us(APP_BUTTON_POLL_US);
    }
    return 1;
}

/** 初始化四个 PS EMIO 按键为输入且关闭输出驱动。 */
int button_input_init(button_input_t *buttons)
{
    XGpioPs_Config *config;

    if (buttons == 0) {
        return XST_FAILURE;
    }

    config = XGpioPs_LookupConfig(APP_GPIO_DEVICE_ID);
    if (config == 0 || XGpioPs_CfgInitialize(&buttons->instance, config,
                                              config->BaseAddr) != XST_SUCCESS) {
        return XST_FAILURE;
    }

    XGpioPs_SetDirectionPin(&buttons->instance, BUTTON_START, 0U);
    XGpioPs_SetDirectionPin(&buttons->instance, BUTTON_RESET, 0U);
    XGpioPs_SetDirectionPin(&buttons->instance, BUTTON_PHASE_INC, 0U);
    XGpioPs_SetDirectionPin(&buttons->instance, BUTTON_PHASE_DEC, 0U);
    XGpioPs_SetOutputEnablePin(&buttons->instance, BUTTON_START, 0U);
    XGpioPs_SetOutputEnablePin(&buttons->instance, BUTTON_RESET, 0U);
    XGpioPs_SetOutputEnablePin(&buttons->instance, BUTTON_PHASE_INC, 0U);
    XGpioPs_SetOutputEnablePin(&buttons->instance, BUTTON_PHASE_DEC, 0U);
    return XST_SUCCESS;
}

/** 消费一次启动键按压。 */
int button_input_take_start_press(button_input_t *buttons)
{
    return button_input_take_press(buttons, BUTTON_START);
}

/** 消费一次复位键按压。 */
int button_input_take_reset_press(button_input_t *buttons)
{
    return button_input_take_press(buttons, BUTTON_RESET);
}

/** 消费一次 B 相位增加键按压。 */
int button_input_take_phase_increment_press(button_input_t *buttons)
{
    return button_input_take_press(buttons, BUTTON_PHASE_INC);
}

/** 消费一次 B 相位减少键按压。 */
int button_input_take_phase_decrement_press(button_input_t *buttons)
{
    return button_input_take_press(buttons, BUTTON_PHASE_DEC);
}

/** 读取启动键当前电平，不执行去抖或消费事件。 */
u32 button_input_read_start_level(const button_input_t *buttons)
{
    return XGpioPs_ReadPin(&buttons->instance, BUTTON_START);
}

/** 读取复位键当前电平，不执行去抖或消费事件。 */
u32 button_input_read_reset_level(const button_input_t *buttons)
{
    return XGpioPs_ReadPin(&buttons->instance, BUTTON_RESET);
}

/** 读取相位增加键当前电平，不执行去抖或消费事件。 */
u32 button_input_read_phase_increment_level(const button_input_t *buttons)
{
    return XGpioPs_ReadPin(&buttons->instance, BUTTON_PHASE_INC);
}

/** 读取相位减少键当前电平，不执行去抖或消费事件。 */
u32 button_input_read_phase_decrement_level(const button_input_t *buttons)
{
    return XGpioPs_ReadPin(&buttons->instance, BUTTON_PHASE_DEC);
}
