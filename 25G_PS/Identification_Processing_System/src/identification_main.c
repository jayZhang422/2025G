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
#include "User/include/button_input.h"
#include "User/include/dds_control.h"
#include "User/include/diagnostics.h"
#include "User/algorithms/two_channel_signal_analyzer.h"
#else
#include "User/include/app_runtime.h"
#include "User/include/app_state_machine.h"
#include "User/include/basic_output_ui.h"
#include "User/algorithms/basic_output.h"
#include "User/algorithms/dac_vpp_calibration.h"
#endif

#define TIMER_ID	1
#define DELAY_10_SECONDS	10000UL
#define DELAY_1_SECOND		1000UL
#define TIMER_CHECK_THRESHOLD	9

#define APP_BASIC2_TASK_STACK_DEPTH \
	( configMINIMAL_STACK_SIZE * 2U )
#define APP_BASIC_OUTPUT_TASK_STACK_DEPTH \
	( configMINIMAL_STACK_SIZE * 8U )
/*-----------------------------------------------------------*/

/* The Tx and Rx tasks as described at the top of this file. */
static void prvTxTask( void *pvParameters );
static void prvRxTask( void *pvParameters );
static void vTimerCallback( TimerHandle_t pxTimer );
#if APP_DIAG_FORCE_DDS_TEST
static void prvBasic2DdsTask( void *pvParameters );
#else
static void prvBasicOutputTask( void *pvParameters );
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

#if !APP_DIAG_FORCE_DDS_TEST
static app_runtime_t g_app_runtime;
static app_state_machine_t g_app_state_machine;
static basic_output_ui_t g_basic_output_ui;
static dds_channel_config_t g_basic_channel_a;
static dds_channel_config_t g_basic_channel_b;

/* Measured at 1 kHz after setting the final adjustable DAC gain. */
static const dac_vpp_calibration_curve_t g_basic_dac_calibration = {
	{
		{0U, 0.0f},
		{1024U, 0.346f},
		{2048U, 0.690f},
		{4096U, 1.390f},
		{6144U, 2.070f},
		{8191U, 2.750f},
		{16383U, 5.470f}
	},
	7U
};
#endif

int main( void )
{

#if APP_DIAG_FORCE_DDS_TEST
	/*
	 * Explicitly isolate the Basic 2 analogue path from ADC/DMA/FFT. The
	 * legacy Hello World tasks are disabled so UART output stays focused.
	 */
	xil_printf( "Basic 2 DDS bring-up mode: %s\r\n", APP_DIAG_BUILD_TAG );
	configASSERT( xTaskCreate( prvBasic2DdsTask,
							  ( const char * ) "Basic2DDS",
							  APP_BASIC2_TASK_STACK_DEPTH,
							  NULL,
							  tskIDLE_PRIORITY + 1,
							  NULL ) == pdPASS );
#else
	configASSERT( xTaskCreate( prvBasicOutputTask,
							  ( const char * ) "BasicOutput",
							  APP_BASIC_OUTPUT_TASK_STACK_DEPTH,
							  NULL,
							  tskIDLE_PRIORITY + 1,
							  NULL ) == pdPASS );
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
#if !APP_DIAG_FORCE_DDS_TEST
static void prvBasicOutputReportSettings( void )
{
	xil_printf( "BASIC34 frequency=%d Hz target=%d mV\r\n",
				(int)g_basic_output_ui.request.frequency_hz,
				(int)( g_basic_output_ui.request.target_output_vpp * 1000.0f ) );
}

static int prvBasicOutputStart( void )
{
	open_loop_output_plan_t plan;
	signal_component_t sine;
	int status;

	status = basic_output_plan( &g_basic_dac_calibration,
								&g_basic_output_ui.request, &plan );
	if (status != XST_SUCCESS) {
		xil_printf( "BASIC34 start blocked: DAC calibration required\r\n" );
		return XST_FAILURE;
	}

	sine.frequency_hz = g_basic_output_ui.request.frequency_hz;
	sine.fundamental_amplitude = 1.0f;
	sine.measured_phase_rad = 0.0f;
	sine.waveform = SIGNAL_WAVE_SINE;
	dds_control_from_component( &sine, 0.0f, &g_basic_channel_a );
	dds_control_from_component( &sine, APP_DDS_B_PHASE_COMPENSATION_DEGREES,
								&g_basic_channel_b );
	g_basic_channel_a.amplitude_code = plan.amplitude_code;
	g_basic_channel_b.amplitude_code = plan.amplitude_code;

	status = dds_control_commit( &g_app_runtime.dds,
								 &g_basic_channel_a, &g_basic_channel_b, 1, 1 );
	if (status != XST_SUCCESS) {
		xil_printf( "BASIC34 DDS start failed\r\n" );
		return XST_FAILURE;
	}

	xil_printf( "BASIC34 started: f=%d Hz target=%d mV input=%d mV amp=%d\r\n",
				(int)g_basic_output_ui.request.frequency_hz,
				(int)( g_basic_output_ui.request.target_output_vpp * 1000.0f ),
				(int)( plan.required_input_vpp * 1000.0f ),
				(int)plan.amplitude_code );
	return XST_SUCCESS;
}

static void prvBasicOutputTask( void *pvParameters )
{
	int status;

	( void )pvParameters;
	app_state_machine_init( &g_app_state_machine );
	status = basic_output_ui_init( &g_basic_output_ui,
								  APP_BASIC34_INITIAL_FREQUENCY_HZ,
								  APP_BASIC34_INITIAL_TARGET_VPP );
	if (status != 0) {
		xil_printf( "BASIC34 UI init failed\r\n" );
		vTaskDelete( NULL );
		return;
	}

	status = app_runtime_init( &g_app_runtime );
	if (status == XST_SUCCESS) {
		status = app_runtime_run_algorithm_self_tests();
	}
	if (status != XST_SUCCESS) {
		app_state_machine_dispatch( &g_app_state_machine,
									APP_EVENT_FAILURE, status );
		xil_printf( "BASIC34 runtime init/self-test failed\r\n" );
		vTaskDelete( NULL );
		return;
	}

	app_state_machine_dispatch( &g_app_state_machine, APP_EVENT_INIT_OK, 0 );
	xil_printf( "BASIC34 ready: START runs, STOP/BACK stops, LEARN requests learning, RESET resets\r\n" );
	prvBasicOutputReportSettings();

	for ( ;; ) {
		if (button_input_take_system_reset_press( &g_app_runtime.buttons )) {
			if (g_basic_output_ui.running) {
				dds_control_commit( &g_app_runtime.dds,
									&g_basic_channel_a, &g_basic_channel_b, 0, 0 );
			}
			basic_output_ui_handle( &g_basic_output_ui, BASIC_OUTPUT_UI_RESET );
			app_state_machine_init( &g_app_state_machine );
			app_state_machine_dispatch( &g_app_state_machine,
									APP_EVENT_INIT_OK, 0 );
			xil_printf( "BASIC34 reset to menu\r\n" );
			prvBasicOutputReportSettings();
		} else if (g_app_state_machine.state == APP_STATE_MENU) {
			if (button_input_take_stop_back_press( &g_app_runtime.buttons )) {
				xil_printf( "BASIC34 menu\r\n" );
			} else if (button_input_take_learn_press( &g_app_runtime.buttons )) {
				app_state_machine_dispatch( &g_app_state_machine,
											APP_EVENT_START_LEARN, 0 );
				xil_printf( "LEARN requested: runtime integration pending; STOP/BACK returns\r\n" );
			} else if (button_input_take_start_press( &g_app_runtime.buttons )) {
				if (prvBasicOutputStart() == XST_SUCCESS) {
					basic_output_ui_handle( &g_basic_output_ui,
											BASIC_OUTPUT_UI_START );
					app_state_machine_dispatch( &g_app_state_machine,
											APP_EVENT_START_BASIC, 0 );
				}
			}
		} else if (g_app_state_machine.state == APP_STATE_BASIC) {
			if (button_input_take_stop_back_press( &g_app_runtime.buttons )) {
				status = dds_control_commit( &g_app_runtime.dds,
										 &g_basic_channel_a, &g_basic_channel_b, 0, 0 );
				basic_output_ui_handle( &g_basic_output_ui,
										BASIC_OUTPUT_UI_RESET );
				app_state_machine_dispatch( &g_app_state_machine,
											APP_EVENT_COMPLETE, 0 );
				xil_printf( status == XST_SUCCESS ?
							"BASIC34 stopped\r\n" : "BASIC34 stop failed\r\n" );
			}
		} else if (g_app_state_machine.state == APP_STATE_LEARN) {
			if (button_input_take_stop_back_press( &g_app_runtime.buttons )) {
				app_state_machine_dispatch( &g_app_state_machine,
											APP_EVENT_RESET, 0 );
				xil_printf( "LEARN cancelled: menu\r\n" );
			}
		}

		vTaskDelay( pdMS_TO_TICKS( 20UL ) );
	}
}
#endif
#if APP_DIAG_FORCE_DDS_TEST
static void prvBasic2DdsTask( void *pvParameters )
{
	button_input_t buttons;
	dds_control_t dds;
	dds_channel_config_t channel_a;
	dds_channel_config_t channel_b;
	signal_component_t sine;
	int status;
	int running = 0;

	(void)pvParameters;
	sine.frequency_hz = BASIC2_BRINGUP_FREQUENCY_HZ;
	sine.fundamental_amplitude = 1.0f;
	sine.measured_phase_rad = 0.0f;
	sine.waveform = SIGNAL_WAVE_SINE;

	dds_control_init( &dds );
	if (button_input_init( &buttons ) != XST_SUCCESS) {
		xil_printf( "Basic 2 button init FAILED\r\n" );
		vTaskDelete( NULL );
		return;
	}
	dds_control_from_component( &sine, 0.0f, &channel_a );
	dds_control_from_component( &sine, APP_DDS_B_PHASE_COMPENSATION_DEGREES,
								&channel_b );
	channel_a.amplitude_code = APP_DIAG_FORCE_DDS_AMPLITUDE;
	channel_b.amplitude_code = APP_DIAG_FORCE_DDS_AMPLITUDE;

	xil_printf( "Basic 2 ready: START runs, STOP/BACK stops, LEARN reports, RESET resets\r\n" );
	xil_printf( "Basic 2 settings: %d Hz, amp_code=%d\r\n",
				(int)BASIC2_BRINGUP_FREQUENCY_HZ,
				(int)APP_DIAG_FORCE_DDS_AMPLITUDE );

	for ( ;; ) {
		if (button_input_take_system_reset_press( &buttons )) {
			if (running) {
				dds_control_commit( &dds, &channel_a, &channel_b, 0, 0 );
			}
			running = 0;
			dds_control_init( &dds );
			xil_printf( "Basic 2 reset: ready\r\n" );
		} else if (button_input_take_stop_back_press( &buttons )) {
			if (running) {
				status = dds_control_commit( &dds, &channel_a, &channel_b, 0, 0 );
				running = 0;
				xil_printf( status == XST_SUCCESS ?
							"Basic 2 stopped\r\n" : "Basic 2 stop failed\r\n" );
			} else {
				xil_printf( "Basic 2 already stopped\r\n" );
			}
		} else if (button_input_take_learn_press( &buttons )) {
			xil_printf( "LEARN pressed: unavailable in Basic 2 diagnostic mode\r\n" );
		} else if (button_input_take_start_press( &buttons )) {
			if (running) {
				xil_printf( "Basic 2 already running\r\n" );
			} else {
				status = dds_control_commit( &dds, &channel_a, &channel_b, 1, 1 );
				if (status != XST_SUCCESS) {
					xil_printf( "Basic 2 DDS start FAILED\r\n" );
				} else {
					running = 1;
					diagnostics_report_dds_snapshot( "BASIC2", &dds );
					xil_printf( "Basic 2 started: %d Hz, amp_code=%d\r\n",
							(int)BASIC2_BRINGUP_FREQUENCY_HZ,
							(int)APP_DIAG_FORCE_DDS_AMPLITUDE );
				}
			}
		}
		vTaskDelay( pdMS_TO_TICKS( 20UL ) );
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

