# Verification Record

Portable handoff baseline: `calvin-uart-integration` commit
`f3ec5557f2cc8cfcb7c5a539cc9442e2141da89d`. The evidence below covers the
subsequent active-project integration candidate; physical board loopback is
still pending.

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

### Active 25G Vivado Integration

`integrate_25g_project.tcl` added UARTLite only as `M05_AXI`, kept `M00` through
`M04` and the DMA/IQ interrupt concat unchanged, enabled PS SD0 on MIO40..45
with MIO47 card detect, retired only the physical DAC boundary, and regenerated
the wrapper. The active UART address is `0x43C30000/64 KiB`; its interrupt is
unconnected.

The first full build stopped before top synthesis because Vivado considered
the generated `system_auto_pc_0` synthesis product missing. The reproducible
build now runs `generate_target all` before resetting and launching runs. The
rerun passed with marker `PL_HMI_UART_CANDIDATE_BUILD_PASSED`.

Final Vivado 2020.2 results:

- synthesis: 0 errors, 0 critical warnings;
- implementation and bitstream: completed successfully;
- post-route timing: WNS `+3.358 ns`, WHS `+0.036 ns`, all user timing
  constraints met;
- `check_timing`: zero no-clock, constant-clock, unconstrained internal
  endpoint, multiple-clock, loop, and partial-delay findings;
- DRC: 0 errors, 147 warnings, 6 advisories;
- bus-skew constraints: four checks, minimum slack `+8.352 ns`;
- `report_io`: F16 is `i_hmi_uart_rx`, input, LVCMOS33; F17 is
  `o_hmi_uart_tx`, output, LVCMOS33;
- UART CDC: one `CDC-3 Info` entry for the core's internal RX double-register
  synchronizer and no UART Critical/Warning entry.

The 147 DRC warnings and full-design CDC Critical findings are in inherited
ADC/FIFO monitor, DDC/FIR, IQ, BRAM/DDS, and reset structures. They are not
hidden or called clean CDC, and the UART integration does not modify those
modules.

Candidate artifacts:

- bit SHA-256
  `F7C335EDEB167BC6B78799B5115C2C254669562DEE62E08EFAA9D6C5368DCD3C`;
- XSA SHA-256
  `7F8812DC50A35357633BBD98D6927D926BB2B59F38176D7BB54C75C5E235FF7C`;
- the XSA-embedded bit hash matches the standalone bit exactly;
- repository `25G_PL/top.xsa` remains unchanged at SHA-256
  `D66E08D01CA66C72D1FB3A3E77106BA18F173BA0D7BB74E76C1409BBCD243497`.

Reports and generated artifacts are under the independent local candidate
directory `tmp/hardware/jay_2025G_pl_hmi_uart_candidate`; they are not treated
as board evidence and are not committed as source.

### Vitis 2020.2 Standalone Loopback Build

The candidate XSA generated a fresh standalone platform and BSP. Direct audit
of `xparameters.h` and `xuartlite_g.c` confirmed:

- one UARTLite instance;
- canonical macros `XPAR_UARTLITE_0_*`;
- device ID 0, base `0x43C30000`, high address `0x43C3FFFF`;
- 115200 baud, 8 data bits, no parity;
- `XUartLite_ConfigTable` uses those canonical macros;
- stdin/stdout remain PS UART1 at `0xE0001000`;
- no UARTLite interrupt was added.

`software/board_test/pl_hmi_uart_loopback.c` first passed a Cortex-A9
`-O2 -Wall -Wextra -Werror` compile. Then
`vitis/create_loopback_test.tcl` created an independent platform/BSP and linked
the Debug ELF with `-O2`, emitting `PL_HMI_UART_LOOPBACK_BUILD_PASSED`.

- source SHA-256
  `2C2E8EE0FC05125B8E2D11EBD5544BE65081C2B69CC7D06CADA1D00B08114AD4`;
- ELF SHA-256
  `982873D0A4BBE530E2A14B78A728CD17E2DDECD5BC57CF9635A8D68969D6F8E2`;
- ELF sections: text 24956 B, data 1160 B, BSS 22584 B.

The test sends 64 changing 16-byte patterns, polls RX while TX drains, checks
framing/parity/overrun status, compares every byte, and has a one-second bound
per iteration. It reports through PS UART1. No existing FreeRTOS source,
system package, `BOOT.BIN`, or repository XSA was generated or overwritten.

## Remaining Board Checks

1. Power off, jumper J11 pin 3/F17 TX to pin 4/F16 RX, then run the exact
   candidate bit/XSA/ELF and capture PS UART1 through the PASS marker.
2. The provided 9-byte event parser is a historical link contract. The current
   problem's TJC pages, component IDs, and event map are not yet defined.
3. TJC TX voltage, display communication, throughput,
   and the required touch-to-stable-display time remain unverified.
4. After any reset of the FCLK0 peripheral domain, software must reinitialize
   UARTLite and send the HMI resynchronization sequence.
