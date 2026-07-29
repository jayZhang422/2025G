#include <stdint.h>

#include "xparameters.h"
#include "xstatus.h"
#include "xtime_l.h"
#include "xuartlite.h"
#include "xuartlite_l.h"
#include "xil_printf.h"

#ifndef XPAR_UARTLITE_0_DEVICE_ID
#error "The active BSP does not contain UARTLite instance 0"
#endif

#if XPAR_XUARTLITE_NUM_INSTANCES != 1U
#error "The loopback test expects exactly one UARTLite instance"
#endif

#if XPAR_UARTLITE_0_BAUDRATE != 115200U
#error "UARTLite must be generated for 115200 baud"
#endif

#if XPAR_UARTLITE_0_DATA_BITS != 8U
#error "UARTLite must be generated for 8 data bits"
#endif

#if XPAR_UARTLITE_0_USE_PARITY != 0U
#error "UARTLite parity must be disabled"
#endif

#define LOOPBACK_PATTERN_BYTES 16U
#define LOOPBACK_ITERATIONS    64U
#define LOOPBACK_TIMEOUT_TICKS ((XTime)COUNTS_PER_SECOND)
#define LOOPBACK_ERROR_MASK    (XUL_SR_PARITY_ERROR | XUL_SR_FRAMING_ERROR | \
                                XUL_SR_OVERRUN_ERROR)

enum loopback_status {
    LOOPBACK_OK = 0,
    LOOPBACK_TIMEOUT = 1,
    LOOPBACK_UART_ERROR = 2,
    LOOPBACK_DATA_MISMATCH = 3
};

static int timeout_expired(XTime start)
{
    XTime now;

    XTime_GetTime(&now);
    return (now - start) > LOOPBACK_TIMEOUT_TICKS;
}

static void make_pattern(uint8_t *buffer, unsigned int iteration)
{
    unsigned int index;

    for (index = 0U; index < LOOPBACK_PATTERN_BYTES; ++index) {
        buffer[index] = (uint8_t)(0xA5U ^
            (uint8_t)(iteration * 17U + index * 29U));
    }
}

static int compare_pattern(const uint8_t *expected,
                           const uint8_t *actual,
                           unsigned int *mismatch_index)
{
    unsigned int index;

    for (index = 0U; index < LOOPBACK_PATTERN_BYTES; ++index) {
        if (expected[index] != actual[index]) {
            *mismatch_index = index;
            return LOOPBACK_DATA_MISMATCH;
        }
    }
    return LOOPBACK_OK;
}

static int run_loopback_iteration(XUartLite *uart,
                                  const uint8_t *tx_buffer,
                                  uint8_t *rx_buffer,
                                  unsigned int *sent_out,
                                  unsigned int *received_out,
                                  uint32_t *status_out)
{
    XTime start;
    unsigned int sent = 0U;
    unsigned int received = 0U;
    uint32_t status = 0U;

    XUartLite_ResetFifos(uart);
    XTime_GetTime(&start);

    while (sent < LOOPBACK_PATTERN_BYTES ||
           received < LOOPBACK_PATTERN_BYTES ||
           XUartLite_IsSending(uart) != 0) {
        if (sent < LOOPBACK_PATTERN_BYTES) {
            sent += XUartLite_Send(uart,
                (uint8_t *)(uintptr_t)(tx_buffer + sent),
                LOOPBACK_PATTERN_BYTES - sent);
        }

        if (received < LOOPBACK_PATTERN_BYTES) {
            received += XUartLite_Recv(uart,
                rx_buffer + received,
                LOOPBACK_PATTERN_BYTES - received);
        }

        status = XUartLite_GetStatusReg(uart->RegBaseAddress);
        if ((status & LOOPBACK_ERROR_MASK) != 0U) {
            *sent_out = sent;
            *received_out = received;
            *status_out = status;
            return LOOPBACK_UART_ERROR;
        }
        if (timeout_expired(start) != 0) {
            *sent_out = sent;
            *received_out = received;
            *status_out = status;
            return LOOPBACK_TIMEOUT;
        }
    }

    *sent_out = sent;
    *received_out = received;
    *status_out = status;
    return LOOPBACK_OK;
}

int main(void)
{
    XUartLite uart;
    XUartLite_Config *config;
    uint8_t tx_buffer[LOOPBACK_PATTERN_BYTES];
    uint8_t rx_buffer[LOOPBACK_PATTERN_BYTES];
    unsigned int iteration;

    config = XUartLite_LookupConfig(XPAR_UARTLITE_0_DEVICE_ID);
    if (config == 0 || config->RegBaseAddr != XPAR_UARTLITE_0_BASEADDR) {
        xil_printf("PL_HMI_UART_LOOPBACK_FAIL stage=config\r\n");
        return XST_FAILURE;
    }
    if (config->BaudRate != XPAR_UARTLITE_0_BAUDRATE ||
        config->DataBits != XPAR_UARTLITE_0_DATA_BITS ||
        config->UseParity != XPAR_UARTLITE_0_USE_PARITY) {
        xil_printf("PL_HMI_UART_LOOPBACK_FAIL stage=format\r\n");
        return XST_FAILURE;
    }
    if (XUartLite_CfgInitialize(&uart, config, config->RegBaseAddr) !=
        XST_SUCCESS) {
        xil_printf("PL_HMI_UART_LOOPBACK_FAIL stage=initialize\r\n");
        return XST_FAILURE;
    }
    if (XUartLite_SelfTest(&uart) != XST_SUCCESS) {
        xil_printf("PL_HMI_UART_LOOPBACK_FAIL stage=self_test\r\n");
        return XST_FAILURE;
    }

    XUartLite_ResetFifos(&uart);
    xil_printf("PL_HMI_UART_LOOPBACK_READY base=0x%08lx baud=%lu "
               "pattern_bytes=%u iterations=%u\r\n",
               (unsigned long)config->RegBaseAddr,
               (unsigned long)config->BaudRate,
               LOOPBACK_PATTERN_BYTES,
               LOOPBACK_ITERATIONS);

    for (iteration = 0U; iteration < LOOPBACK_ITERATIONS; ++iteration) {
        unsigned int index;
        unsigned int sent;
        unsigned int received;
        unsigned int mismatch_index = 0U;
        uint32_t uart_status;
        int status;

        make_pattern(tx_buffer, iteration);
        for (index = 0U; index < LOOPBACK_PATTERN_BYTES; ++index)
            rx_buffer[index] = 0U;

        status = run_loopback_iteration(&uart, tx_buffer, rx_buffer,
                                        &sent, &received, &uart_status);
        if (status != LOOPBACK_OK) {
            xil_printf("PL_HMI_UART_LOOPBACK_FAIL iteration=%u code=%d "
                       "sent=%u received=%u status=0x%02lx\r\n",
                       iteration, status, sent, received,
                       (unsigned long)uart_status);
            return XST_FAILURE;
        }

        status = compare_pattern(tx_buffer, rx_buffer, &mismatch_index);
        if (status != LOOPBACK_OK) {
            xil_printf("PL_HMI_UART_LOOPBACK_FAIL iteration=%u code=%d "
                       "index=%u expected=0x%02x actual=0x%02x\r\n",
                       iteration, status, mismatch_index,
                       tx_buffer[mismatch_index], rx_buffer[mismatch_index]);
            return XST_FAILURE;
        }

        if (((iteration + 1U) % 8U) == 0U) {
            xil_printf("PL_HMI_UART_LOOPBACK_PROGRESS %u/%u\r\n",
                       iteration + 1U, LOOPBACK_ITERATIONS);
        }
    }

    xil_printf("PL_HMI_UART_LOOPBACK_PASS iterations=%u bytes=%u\r\n",
               LOOPBACK_ITERATIONS,
               LOOPBACK_ITERATIONS * LOOPBACK_PATTERN_BYTES);
    return XST_SUCCESS;
}
