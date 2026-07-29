# PS/PL Hardware Contract

This file is the PS-side contract for the active `25G_PL` hardware. Read it
with the BSP generated from the same XSA before changing PS software. The
generated `xparameters.h` remains authoritative for addresses, device IDs,
and interrupt IDs.

## Candidate Provenance

- Tool: Vivado/Vitis 2020.2 only.
- Device: `xc7z020clg400-2` on ALINX AX7020.
- Candidate bitstream:
  `tmp/hardware/jay_2025G_pl_hmi_uart_candidate/25g_pl_hmi_uart.bit`
  in the integration workspace, SHA-256
  `F7C335EDEB167BC6B78799B5115C2C254669562DEE62E08EFAA9D6C5368DCD3C`.
- Candidate XSA:
  `tmp/hardware/jay_2025G_pl_hmi_uart_candidate/25g_pl_hmi_uart.xsa`,
  SHA-256
  `7F8812DC50A35357633BBD98D6927D926BB2B59F38176D7BB54C75C5E235FF7C`.
- The XSA-embedded bitstream hash exactly matches the standalone bitstream.
- The repository's pre-existing `25G_PL/top.xsa` is not this candidate and was
  not overwritten.

These hashes describe the current offline integration candidate. They are not
board-loopback or display verification.

## Control Plane

PS `M_AXI_GP0` feeds `axi_interconnect_0`. The UART addition expands the
interconnect from five to six master interfaces without changing the existing
connections:

| Interconnect port | Target | BSP address in this candidate |
| --- | --- | --- |
| `M00_AXI` | `axi_dma_adc/S_AXI_LITE` | `0x40400000..0x4040FFFF` |
| `M01_AXI` | `axi_bram_ctrl_0/S_AXI` | `0x40000000..0x40001FFF` |
| `M02_AXI` | `iq_demodulator_0/s_axi` | `0x43C00000..0x43C0FFFF` |
| `M03_AXI` | `ad_fifo_monitor_axi_0` | `0x43C10000..0x43C1FFFF` |
| `M04_AXI` | `ddc_stream_0/s_axi` | `0x43C20000..0x43C20FFF` |
| `M05_AXI` | `pl_hmi_uart_0/S_AXI` | `0x43C30000..0x43C3FFFF` |

Do not copy these literals into application code. The generated BSP for this
candidate provides the canonical UARTLite macros:

```c
XPAR_UARTLITE_0_DEVICE_ID
XPAR_UARTLITE_0_BASEADDR
XPAR_UARTLITE_0_HIGHADDR
XPAR_UARTLITE_0_BAUDRATE
XPAR_UARTLITE_0_USE_PARITY
XPAR_UARTLITE_0_ODD_PARITY
XPAR_UARTLITE_0_DATA_BITS
```

It also provides hierarchy-specific aliases beginning with
`XPAR_PL_HMI_UART_0_AXI_UARTLITE_CORE_`. The generated
`XUartLite_ConfigTable` uses the canonical `XPAR_UARTLITE_0_*` macros.

## PL HMI UART Contract

| Item | Verified candidate value |
| --- | --- |
| Vendor IP | Xilinx AXI UARTLite 2.0; BSP driver UARTLite 3.5 |
| AXI clock | `processing_system7_0/FCLK_CLK0`, 100 MHz |
| AXI reset | `proc_sys_reset_0/peripheral_aresetn`, active low |
| Serial format | 115200 baud, 8 data bits, no parity, one stop bit |
| Wrapper RX | `PL_HMI_UART_rxd` |
| Wrapper TX | `PL_HMI_UART_txd` |
| Top RX | `i_hmi_uart_rx`, J11 pin 4 / FPGA F16 / LVCMOS33 |
| Top TX | `o_hmi_uart_tx`, J11 pin 3 / FPGA F17 / LVCMOS33 |
| Signal ground | J11 pin 1 |
| Interrupt | Deliberately unconnected; first integration is polling |

J11 BANK35 is 3.3 V. Do not connect a measured 5 V logic output to F16. The
TJC display TX level must be measured under its actual 5 V supply or converted
before direct connection. Do not power the 7-inch display from J11 without a
separate current-capacity design.

The UART shares the FCLK0 peripheral reset domain. Software must reinitialize
UARTLite and resynchronize the HMI protocol after any reset that asserts
`peripheral_aresetn`.

## Existing Data And Interrupt Paths

- ADC/FIFO/AXIS/DMA, DDC/FIR, IQ, and BRAM/DDS behavior are outside the UART
  change and retain their existing interfaces.
- DMA remains S2MM-only from the application perspective and keeps its current
  AXI stream and HP0 memory path.
- The current BSP assigns DMA S2MM interrupt ID 61 and IQ interrupt ID 62.
- `xlconcat_0/In0` remains DMA S2MM, `In1` remains IQ, and its output remains
  connected to PS `IRQ_F2P`.
- UARTLite `interrupt` is not connected to `xlconcat_0`; no UARTLite fabric
  interrupt macro is expected in this BSP.

Do not modify the DMA/IQ interrupt concat for the first UART loopback or HMI
bring-up. Polling avoids disturbing the existing FreeRTOS ownership model.

## PS Peripherals And Console

- The standalone BSP uses `ps7_uart_1` at `0xE0001000` for both stdin and
  stdout. Keep UART1 as the diagnostic console while testing the PL UART.
- PS SD0 is enabled for final onboard Micro SD boot. The verified mapping is
  MIO40 clock, MIO41 command, MIO42..45 data, and MIO47 card detect.
- The generated BSP exposes `ps7_sd_0` and reports card detect present.
- No `BOOT.BIN` was generated during this integration.

## First Board Acceptance

1. Keep the TJC display disconnected.
2. With board power off, jumper J11 pin 3/F17 TX to J11 pin 4/F16 RX and use
   J11 pin 1 only as signal ground when needed.
3. Program the candidate bitstream and run the standalone loopback ELF built
   from this exact XSA/BSP.
4. Observe progress and PASS/FAIL only on PS UART1. The test must use bounded
   timeouts and compare a 16-byte pattern repeatedly.
5. Remove the loopback jumper before connecting a display.

After physical loopback passes, configure the TJC project to 115200 8N1,
verify its TX voltage, and then test command terminators and touch-event
frames. The historical nine-byte event envelope is transport evidence only;
current page names and component IDs must come from the current TJC project.

## Verification Boundary And Known Risks

- Vivado 2020.2 synthesis and implementation passed with WNS `+3.358 ns`, WHS
  `+0.036 ns`, zero DRC errors, and correctly placed F16/F17 UART ports.
- The full inherited design still reports CDC critical findings and 147 DRC
  warnings in existing ADC/FIFO monitor, DDC/FIR, IQ, BRAM/DDS, and reset
  structures. The only UART path in the CDC report is one `CDC-3 Info` entry
  for the AXI UARTLite RX input's internal double-register synchronizer; no
  UART path is Critical or Warning. The inherited findings are not waived by
  the UART integration and must not be described as clean CDC.
- Physical UART loopback, TJC voltage, display traffic, and end-to-end
  touch-to-stable-display time remain board tests.
- The team-leader-owned algorithm and `/3` FIR are not modified by this
  contract.
