# Portable PL HMI UART Handoff

This directory is a source-only handoff for a TJC-compatible PL UART. It does
not modify the active `25G_PL` Block Design, top-level RTL, constraints, DMA,
DDC/FIR, IQ, or PS application.

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

## Current 25G Integration Boundary

The current `25G_PL` control interconnect has five occupied master interfaces:
BRAM, DMA, IQ, FIFO monitor, and DDC. Integrating this UART requires a reviewed
parent-BD change to add another AXI-Lite target. Use FCLK0 and
`proc_sys_reset_0/peripheral_aresetn`; leave `interrupt` unconnected for the
first polled build so the existing DMA/IQ IRQ concat is unchanged.

Do not hard-code the resulting UARTLite address or device ID in software. Read
the generated `XPAR_*` values from the BSP created from the new XSA.

## J11 Pin Conflict

The intended final pins are:

- TX: AX7020 J11 pin 3, FPGA `F17`
- RX: AX7020 J11 pin 4, FPGA `F16`
- signal ground: AX7020 J11 pin 1

The current 25G project still has an active DAC top-level and an active
`da_hw_275.xdc`. Its DAC bus already uses `F16` and `F17`. Therefore the
provided XDC is intentionally named `.example` and must not be added until the
team explicitly retires or remaps the conflicting DAC interface and verifies
that no active constraint still owns either pin.

J11 BANK35 is 3.3 V. Confirm the display TX voltage or add level conversion
before connecting it to the FPGA RX input. Do not power the 7-inch display from
J11 without a separately verified current-capacity design.

## Contents

- `vivado/pl_hmi_uart_bd.tcl`: source-defined hierarchy around Xilinx
  AXI UARTLite 2.0.
- `vivado/tests/validate_pl_hmi_uart_bd.tcl`: in-memory Vivado 2020.2 BD
  validation.
- `vivado/constraints/AX7020_J11_uart.xdc.example`: inactive candidate
  constraints with conflict warnings.
- `software/include` and `software/src`: polled XUartLite transport and the
  historical 9-byte display-event parser.
- `software/tests/test_hmi_protocol.c`: host regression for parser resync and
  frame decoding.
- `VERIFICATION.md`: exact offline evidence and remaining integration risks.

The historical event envelope is `55 AA CMD VIEW DATA[31:0] FF`, little-endian
data. It is evidence from the previous display project, not the component/page
contract for the current problem. Replace only the UI-specific protocol layer
after the new TJC project defines its events; the UARTLite hardware hierarchy
does not depend on that parser.

## Verification Boundary

The included checks can prove Tcl construction, IP configuration, BD
validation, and protocol parsing. They do not prove synthesis, implementation,
pin legality after DAC removal, physical loopback, display voltage, or TJC
throughput. The first board acceptance is an F17/F16 physical loopback after a
conflict-free implemented design is available.
