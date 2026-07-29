# Verification Record

Baseline: `calvin-uart-integration` at parent commit
`820466722b561d5b043b16d52faf8ff6b5563758`.

## Passed Offline Checks

### Vivado 2020.2 Block Design

Command:

```powershell
D:\Vivado_install\Vivado\2020.2\bin\vivado.bat `
  -mode batch -nojournal -nolog -notrace `
  -source handoff/pl_hmi_uart/vivado/tests/validate_pl_hmi_uart_bd.tcl
```

Result: PASS with marker `PL_HMI_UART_BD_VALIDATION_PASSED`. The final run used
Vivado 2020.2 build 3064766, constructed the hierarchy, assigned the isolated
test address space, validated the BD, and confirmed:

- AXI UARTLite 2.0
- 115200 baud
- 8 data bits
- parity disabled
- 100 MHz AXI clock metadata

The final run had no Critical Warning. This is not synthesis, implementation,
timing, or board evidence.

### Host Protocol Regression

The handoff protocol source and test were compiled with `-std=c11 -Wall
-Wextra -Werror`. Result: PASS with marker `HMI_PROTOCOL_C_TEST_PASSED`.

### Cortex-A9 Driver Compile

`pl_hmi_uart.c` was compiled with the Vitis 2020.2 Cortex-A9 GCC, `-O2 -Wall
-Wextra -Werror -mcpu=cortex-a9`, the Xilinx UARTLite 3.5 driver headers, and
the current project BSP headers for platform/clock definitions. Result: PASS
at compile boundary.

The current XSA does not contain UARTLite, so no generated UARTLite `XPAR_*`
macro, driver configuration table entry, application link, or target execution
is claimed.

## Known Integration Blockers

1. Active `25G_PL/25G_PL.srcs/constrs_1/new/da_hw_275.xdc` uses `F16` and
   `F17` for the DAC bus. The UART candidate must not claim these pins until
   the team explicitly retires or remaps the DAC interface.
2. The active AXI interconnect currently has five occupied master interfaces.
   Adding UARTLite requires a reviewed parent-BD expansion and a newly exported
   XSA/BSP.
3. The provided 9-byte event parser is a historical link contract. The current
   problem's TJC pages, component IDs, and event map are not yet defined.
4. TJC TX voltage, F17/F16 physical loopback, display communication, throughput,
   and the required touch-to-stable-display time remain unverified.
