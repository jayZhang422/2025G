/*
    Copyright (C) 2017 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
    Copyright (c) 2012 - 2020 Xilinx, Inc. All Rights Reserved.
	SPDX-License-Identifier: MIT


    http://www.FreeRTOS.org
    http://aws.amazon.com/freertos


    1 tab == 4 spaces!
*/

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"
/* Xilinx includes. */
#include "xil_printf.h"
#include "xparameters.h"

/* Project hardware glue. */
#include "User/include/app_config.h"
#if APP_DIAG_FORCE_DDS_TEST
#include "User/include/dds_control.h"
#include "User/include/diagnostics.h"
#include "User/algorithms/two_channel_signal_analyzer.h"
#endif

#define TIMER_ID	1
#define DELAY_10_SECONDS	10000UL
#define DELAY_1_SECOND		1000UL
#define TIMER_CHECK_THRESHOLD	9
/*-----------------------------------------------------------*/

/* The Tx and Rx tasks as described at the top of this file. */
static void prvTxTask( void *pvParameters );
static void prvRxTask( void *pvParameters );
static void vTimerCallback( TimerHandle_t pxTimer );
#if APP_DIAG_FORCE_DDS_TEST
static void prvBasic2DdsTask( void *pvParameters );
#endif
/*-----------------------------------------------------------*/

/* The queue used by the Tx and Rx tasks, as described at the top of this
file. */
static TaskHandle_t xTxTask;
static TaskHandle_t xRxTask;
static QueueHandle_t xQueue = NULL;
static TimerHandle_t xTimer = NULL;
char HWstring[15] = "Hello World";
long RxtaskCntr = 0;

#if APP_DIAG_FORCE_DDS_TEST
/*
 * This is a bench-only value for the Basic 2 bring-up path. 1 kHz is also the
 * explicit frequency used by the contest's Basic 3 item; it is not a product
 * default and does not replace the future user frequency-setting interface.
 */
#define BASIC2_BRINGUP_FREQUENCY_HZ 1000.0f
#endif

int main( void )
{
#if !APP_DIAG_FORCE_DDS_TEST
	const TickType_t x10seconds = pdMS_TO_TICKS( DELAY_10_SECONDS );
#endif

#if APP_DIAG_FORCE_DDS_TEST
	/*
	 * Explicitly isolate the Basic 2 analogue path from ADC/DMA/FFT. The
	 * legacy Hello World tasks are disabled so UART output stays focused.
	 */
	xil_printf( "Basic 2 DDS bring-up mode: %s\r\n", APP_DIAG_BUILD_TAG );
	configASSERT( xTaskCreate( prvBasic2DdsTask,
							  ( const char * ) "Basic2DDS",
							  configMINIMAL_STACK_SIZE,
							  NULL,
							  tskIDLE_PRIORITY + 1,
							  NULL ) == pdPASS );
#endif

#if !APP_DIAG_FORCE_DDS_TEST
	xil_printf( "Hello from Freertos example main\r\n" );
#endif


#if !APP_DIAG_FORCE_DDS_TEST

	/* Create the two tasks.  The Tx task is given a lower priority than the
	Rx task, so the Rx task will leave the Blocked state and pre-empt the Tx
	task as soon as the Tx task places an item in the queue. */
	xTaskCreate( 	prvTxTask, 					/* The function that implements the task. */
					( const char * ) "Tx", 		/* Text name for the task, provided to assist debugging only. */
					configMINIMAL_STACK_SIZE, 	/* The stack allocated to the task. */
					NULL, 						/* The task parameter is not used, so set to NULL. */
					tskIDLE_PRIORITY,			/* The task runs at the idle priority. */
					&xTxTask );

	xTaskCreate( prvRxTask,
				 ( const char * ) "GB",
				 configMINIMAL_STACK_SIZE,
				 NULL,
				 tskIDLE_PRIORITY + 1,
				 &xRxTask );

	/* Create the queue used by the tasks.  The Rx task has a higher priority
	than the Tx task, so will preempt the Tx task and remove values from the
	queue as soon as the Tx task writes to the queue - therefore the queue can
	never have more than one item in it. */
	xQueue = xQueueCreate( 	1,						/* There is only one space in the queue. */
							sizeof( HWstring ) );	/* Each space in the queue is large enough to hold a uint32_t. */

	/* Check the queue was created. */
	configASSERT( xQueue );

	/* Create a timer with a timer expiry of 10 seconds. The timer would expire
	 after 10 seconds and the timer call back would get called. In the timer call back
	 checks are done to ensure that the tasks have been running properly till then.
	 The tasks are deleted in the timer call back and a message is printed to convey that
	 the example has run successfully.
	 The timer expiry is set to 10 seconds and the timer set to not auto reload. */
	xTimer = xTimerCreate( (const char *) "Timer",
							x10seconds,
							pdFALSE,
							(void *) TIMER_ID,
							vTimerCallback);
	/* Check the timer was created. */
	configASSERT( xTimer );

	/* start the timer with a block time of 0 ticks. This means as soon
	   as the schedule starts the timer will start running and will expire after
	   10 seconds */
	xTimerStart( xTimer, 0 );

#endif

	/* Start the tasks and timer running. */
	vTaskStartScheduler();

	/* If all is well, the scheduler will now be running, and the following line
	will never be reached.  If the following line does execute, then there was
	insufficient FreeRTOS heap memory available for the idle and/or timer tasks
	to be created.  See the memory management section on the FreeRTOS web site
	for more details. */
	for( ;; );
}


/*-----------------------------------------------------------*/
#if APP_DIAG_FORCE_DDS_TEST
static void prvBasic2DdsTask( void *pvParameters )
{
	dds_control_t dds;
	dds_channel_config_t channel_a;
	dds_channel_config_t channel_b;
	signal_component_t sine;
	int status;

	(void)pvParameters;
	sine.frequency_hz = BASIC2_BRINGUP_FREQUENCY_HZ;
	sine.fundamental_amplitude = 1.0f;
	sine.measured_phase_rad = 0.0f;
	sine.waveform = SIGNAL_WAVE_SINE;

	dds_control_init( &dds );
	dds_control_from_component( &sine, 0.0f, &channel_a );
	dds_control_from_component( &sine, APP_DDS_B_PHASE_COMPENSATION_DEGREES, &channel_b );
	channel_a.amplitude_code = APP_DIAG_FORCE_DDS_AMPLITUDE;
	channel_b.amplitude_code = APP_DIAG_FORCE_DDS_AMPLITUDE;

	status = dds_control_commit( &dds, &channel_a, &channel_b, 1, 1 );
	if (status != XST_SUCCESS) {
		xil_printf( "Basic 2 DDS bring-up FAILED\r\n" );
		vTaskDelete( NULL );
		return;
	}

	diagnostics_report_dds_snapshot( "BASIC2", &dds );
	xil_printf( "Basic 2 DDS output started: %d Hz, amp_code=%d\r\n", (int)BASIC2_BRINGUP_FREQUENCY_HZ, (int)APP_DIAG_FORCE_DDS_AMPLITUDE );

	for ( ;; ) {
		/* Keep the output running; no repeated COMMIT_SEQ writes are needed. */
		vTaskDelay( pdMS_TO_TICKS( 1000UL ) );
	}
}
#endif

static void prvTxTask( void *pvParameters )
{
const TickType_t x1second = pdMS_TO_TICKS( DELAY_1_SECOND );

	for( ;; )
	{
		/* Delay for 1 second. */
		vTaskDelay( x1second );

		/* Send the next value on the queue.  The queue should always be
		empty at this point so a block time of 0 is used. */
		xQueueSend( xQueue,			/* The queue being written to. */
					HWstring, /* The address of the data being sent. */
					0UL );			/* The block time. */
	}
}

/*-----------------------------------------------------------*/
static void prvRxTask( void *pvParameters )
{
char Recdstring[15] = "";

	for( ;; )
	{
		/* Block to wait for data arriving on the queue. */
		xQueueReceive( 	xQueue,				/* The queue being read. */
						Recdstring,	/* Data is read into this address. */
						portMAX_DELAY );	/* Wait without a timeout for data. */

		/* Print the received data. */
		xil_printf( "Rx task received string from Tx task: %s\r\n", Recdstring );
		RxtaskCntr++;
	}
}

/*-----------------------------------------------------------*/
static void vTimerCallback( TimerHandle_t pxTimer )
{
	long lTimerId;
	configASSERT( pxTimer );

	lTimerId = ( long ) pvTimerGetTimerID( pxTimer );

	if (lTimerId != TIMER_ID) {
		xil_printf("FreeRTOS Hello World Example FAILED");
	}

	/* If the RxtaskCntr is updated every time the Rx task is called. The
	 Rx task is called every time the Tx task sends a message. The Tx task
	 sends a message every 1 second.
	 The timer expires after 10 seconds. We expect the RxtaskCntr to at least
	 have a value of 9 (TIMER_CHECK_THRESHOLD) when the timer expires. */
	if (RxtaskCntr >= TIMER_CHECK_THRESHOLD) {
		xil_printf("Successfully ran FreeRTOS Hello World Example");
	} else {
		xil_printf("FreeRTOS Hello World Example FAILED");
	}

	vTaskDelete( xRxTask );
	vTaskDelete( xTxTask );
}

