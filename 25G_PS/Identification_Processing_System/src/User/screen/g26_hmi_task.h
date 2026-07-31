#ifndef USER_SCREEN_G26_HMI_TASK_H_
#define USER_SCREEN_G26_HMI_TASK_H_

/** FreeRTOS task that exclusively owns the J11 AXI UARTLite screen link. */
void g26_hmi_task(void *parameters);

#endif /* USER_SCREEN_G26_HMI_TASK_H_ */
