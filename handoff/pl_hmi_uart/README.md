# Portable PL HMI UART Handoff

This directory contains the reusable source and verification flow for a
TJC-compatible PL UART. On branch `calvin-uart-integration`, the active
`25G_PL` design now instantiates this hierarchy, while the original DMA,
DDC/FIR, IQ, and PS application interfaces remain unchanged.

## Fixed Contract

| Interface | Direction | Contract |
| --- | --- | --- |
| `S_AXI` | slave | AXI4-Lite register interface; the parent design owns address assignment |
| `s_axi_aclk` | input | 100 MHz by default |
| `s_axi_aresetn` | input | active-low reset associated with `s_axi_aclk` |
| `UART` | master | 115200 baud, 8 data bits, no parity, one stop bit by default |
| `interrupt` | output | optional; the first integration should use polling |

The Tcl procedure accepts optional clock and baud arguments while preserving
100 MHz and 115200 as its defaults:

```tcl
source /path/to/vivado/pl_hmi_uart_bd.tcl
create_pl_hmi_uart_subsystem / pl_hmi_uart_0
```

No generated `.xci`, `.cache`, `.gen`, `.runs`, XSA, BSP, device ID, interrupt
ID, or base address is part of this handoff.

## Current 25G Integration

The active control interconnect now has six master interfaces. UARTLite is the
new `M05_AXI` target at `0x43C30000..0x43C3FFFF`; `M00` through `M04` retain
their previous connections and addresses. The UART uses FCLK0 and
`proc_sys_reset_0/peripheral_aresetn`. Its interrupt remains unconnected for
the first polled build, so the existing DMA/IQ IRQ concat is unchanged.

Do not hard-code the resulting UARTLite address or device ID in software. Read
the generated `XPAR_UARTLITE_0_*` values from the BSP created from the new
XSA. The Vitis 2020.2 BSP generated for this candidate reports device ID 0,
115200 baud, 8 data bits, and no parity.

The PS7 configuration also enables the pre-authorized AX7020 onboard SD0 on
MIO40..45 with card detect on MIO47. PS UART1 remains the diagnostic console.

## J11 And DAC Retirement

The intended final pins are:

- TX: AX7020 J11 pin 3, FPGA `F17`
- RX: AX7020 J11 pin 4, FPGA `F16`
- signal ground: AX7020 J11 pin 1

The active candidate retires the physical DAC ports from `top.v` and removes
their board timing constraints. Existing `H_top` DAC interface names are not
renamed or deleted; their outputs are left open at the parent boundary. The
legacy filename `da_hw_275.xdc` is retained, but its active constraints now
map only `o_hmi_uart_tx` to F17 and `i_hmi_uart_rx` to F16. Post-place
`report_io` confirms those exact directions and `LVCMOS33` I/O standards.

J11 BANK35 is 3.3 V. Confirm the display TX voltage or add level conversion
before connecting it to the FPGA RX input. Do not power the 7-inch display from
J11 without a separately verified current-capacity design.

## Contents

- `vivado/pl_hmi_uart_bd.tcl`: source-defined hierarchy around Xilinx
  AXI UARTLite 2.0.
- `vivado/tests/validate_pl_hmi_uart_bd.tcl`: in-memory Vivado 2020.2 BD
  validation.
- `vivado/constraints/AX7020_J11_uart.xdc.example`: portable inactive pin
  example; the active project uses its legacy-named `da_hw_275.xdc`.
- `software/include` and `software/src`: polled XUartLite transport and the
  historical 9-byte display-event parser.
- `software/board_test/pl_hmi_uart_loopback.c`: standalone 16-byte physical
  loopback test with bounded timeouts and 64 changing patterns.
- `software/tests/test_hmi_protocol.c`: host regression for parser resync and
  frame decoding.
- `vitis/create_loopback_test.tcl`: creates an independent Vitis 2020.2
  standalone platform/BSP/ELF; it does not invoke Bootgen or touch the current
  FreeRTOS application.
- `VERIFICATION.md`: exact offline evidence and remaining integration risks.

The historical event envelope is `55 AA CMD VIEW DATA[31:0] FF`, little-endian
data. It is evidence from the previous display project, not the component/page
contract for the current problem. Replace only the UI-specific protocol layer
after the new TJC project defines its events; the UARTLite hardware hierarchy
does not depend on that parser.

## Rebuild And First Board Test

Set an independent candidate directory before running the Vivado build:

```powershell
$env:PL_HMI_UART_CANDIDATE_DIR = 'D:\path\to\uart_candidate'
D:\Vivado_install\Vivado\2020.2\bin\vivado.bat `
  -mode batch -nojournal -nolog -notrace `
  -source handoff/pl_hmi_uart/vivado/build_25g_uart_candidate.tcl
```

Build the standalone test from that candidate XSA in a separate Vitis
workspace:

```powershell
$env:PL_HMI_UART_XSA_FILE = 'D:\path\to\uart_candidate\25g_pl_hmi_uart.xsa'
$env:PL_HMI_UART_VITIS_WORKSPACE_DIR = 'D:\path\to\uart_loopback_workspace'
D:\Vivado_install\Vitis\2020.2\bin\xsct.bat `
  handoff/pl_hmi_uart/vitis/create_loopback_test.tcl
```

For the first board test, leave the TJC disconnected and power the board off
before jumpering J11 pin 3/F17 to pin 4/F16. Program the candidate bitstream,
run the generated `pl_hmi_uart_loopback.elf`, and observe PS UART1. A passing
run ends with `PL_HMI_UART_LOOPBACK_PASS iterations=64 bytes=1024`. Remove the
jumper before attaching the display.

## Verification Boundary

The active candidate passes Vivado 2020.2 synthesis, implementation, timing,
DRC error checks, I/O placement, XSA export, BSP generation, and standalone
ELF linking. The inherited full design still has non-UART DRC warnings and CDC
critical findings recorded in `VERIFICATION.md`. Offline checks do not prove
physical loopback, display TX voltage, TJC traffic, or display throughput.
