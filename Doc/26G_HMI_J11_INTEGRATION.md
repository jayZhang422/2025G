# 26G HMI J11 Integration

Chinese quick-start, file ownership, wiring, programming, and board-test
instructions are in the repository root `README.md` and
`Doc/26G_HMI_J11_BOARD_TEST.md`.

This handoff integrates the final 2026 G-question TJC display with the
team-leader project without changing the existing algorithm, DDC/FIR, DMA, or
`H_top` interface names. Vivado and Vitis 2020.2 are required.

## 1. Fixed Hardware Contract

| Item | Value |
| --- | --- |
| Display UART | AXI UARTLite v2.0, polling only |
| AXI connection | `axi_interconnect_0/M05_AXI` |
| BSP address | `XPAR_UARTLITE_0_BASEADDR`; current XSA maps `0x43C30000..0x43C3FFFF` |
| Clock/reset | PS `FCLK_CLK0`, 100 MHz; `proc_sys_reset_0/peripheral_aresetn` |
| Serial format | 115200 baud, 8 data bits, no parity, one stop bit |
| J11 TX | pin 3, FPGA F17, `o_hmi_uart_tx`, LVCMOS33 |
| J11 RX | pin 4, FPGA F16, `i_hmi_uart_rx`, LVCMOS33 |
| Signal ground | J11 pin 1 |
| UART interrupt | Deliberately unconnected |

This project does not require a DAC. The former external DAC pins are retired
because F16/F17 are required by the display. The existing `H_top` DAC
interface names and internal logic are kept unchanged and left open at
`top.v` for interface compatibility only.

J11 BANK35 is 3.3 V. Cross the serial signals (`J11 TX -> display RX`,
`display TX -> J11 RX`) and connect ground. Do not power the display from J11.
Keep the adapter USB disconnected during the direct FPGA test unless its
USB-UART isolation has been proven. The display TX level must be safe for a
3.3 V FPGA input under the actual supply condition.

## 2. Portable UART IP

`25G_PL/ip_core/pl_hmi_uart/src/pl_hmi_uart_bd.tcl` defines
`create_pl_hmi_uart_subsystem`. The hierarchy exposes:

- `S_AXI`, `s_axi_aclk`, and `s_axi_aresetn` to the parent design;
- `UART` to the top-level J11 pins;
- `interrupt`, which remains unconnected in this polling implementation.

`25G_PL/script/pl_integrate_hmi_uart.tcl` applies the hierarchy to the current
team-leader Block Design. Before changing anything it asserts the existing
M00..M04 control targets and the PS SD0 MIO40..45/CD47 configuration. It then
adds only M05, assigns the UARTLite address, validates the BD, regenerates its
products and wrapper, and exports `system.tcl`.

AXI UARTLite has a build-time baud rate; software cannot change it at runtime.
Vivado 2020.2 implements the 16x sampling divider as
`round(Fclk / (16 * baud))`. At 100 MHz and 115200 baud the ideal divider is
54.253 and the implemented divider is 54, giving about 115740.741 baud
(+0.469%). Higher baud rates use smaller integer dividers, so a one-count
divider change is a larger relative baud step. For example, 9600 baud is
about +0.006% high at 100 MHz, while 921600 baud rounds to divider 7 and is
about -3.118% low. This quantization is why selecting a higher advertised
baud rate can reduce timing margin even when both endpoints list that rate.
To change the baud rate later, update it as one coordinated change:

1. Change the `baud_rate` argument/default in
   `25G_PL/ip_core/pl_hmi_uart/src/pl_hmi_uart_bd.tcl` and the matching assertions in the PL
   integration/build scripts.
2. Regenerate the BD, rerun implementation, and export a new XSA containing
   the new bitstream.
3. Rebuild the Vitis platform/BSP/application from that exact XSA and update
   the expected baud assertion in `25G_PS/script/ps_build_hmi_candidate.tcl`.
4. Set the TJC project to the same baud rate and reprogram the display.
5. Repeat the physical UART and touch-to-final-display timing tests.

Do not change only the PS source or only the HMI project; either mismatch
breaks communication.

## 3. Final HMI Contract

The tracked display source is `HMI/26G.HMI`, 7,743,543 bytes,
SHA-256:

`E8BA47E75D6E21D494C0E016C71AB861D3B4863A7DF87CB1ABE34B88AEFF8BFC`

It is programmed into the TJC display separately and is not part of an FPGA
bitstream, XSA, ELF, or `BOOT.BIN`.

The abandoned J12/UART0 experiment is not part of this handoff. The final and
only active display transport is J11 AXI UARTLite.

The display sends five-byte touch frames:

`AA 55 PAGE COMMAND FF`

| Page | Action | PAGE/COMMAND |
| --- | --- | --- |
| page2 | START | `02 01` |
| page2 | STOP | `02 00` |
| page2 | 1 period | `02 11` |
| page2 | show time parameters | `02 12` |
| page2 | 3 periods | `02 33` |
| page3 | START | `03 01` |
| page3 | STOP | `03 00` |
| page3 | show amplitudes | `03 11` |

PS commands are ASCII followed by `FF FF FF`. The HMI scripts own page2
`status` and page3 `status1`; PS software must not write either object.

Page2 uses waveform component ID 1, channel 0. The analyzer's 640-point
one/three-period view is linearly resampled to 601 points before `add`
commands. `x0`, `x1`, and `n0` display `Upp` in mV, true RMS in mV, and the
fundamental in kHz. `x0` and `x1` use three fractional digits; PS therefore
writes millivolts multiplied by 1000 to their integer `.val` properties.

Page3 uses `x0..x2` for frequency, `t0..t2` for amplitude, and `t7` for the
optional third-row label. Frequencies are available before the amplitude
button is pressed because they are needed to interpret the qualitative line
spectrum. Amplitude text remains hidden until command `03 11`. The displayed
amplitude is `components[i].amplitude_mv`, the sinusoidal peak value required
by the problem, not peak-to-peak amplitude, formatted with three fractional
digits.

The compressed 1616x976 spectrum background used for calibration has
SHA-256:

`57F586DF7AA8E71E44E28EC85687A0AB40BEF91D94D123853BE12DBC26EB8F9A`

Its screen calibration is fixed in `g26_hmi_render.h`: X coordinates
`71/152/233/314/395/476` represent 0/100/200/300/400/500 kHz, and Y
coordinates `274/217/161/105/48` represent relative amplitude
0/0.25/0.5/0.75/1. Replacing or resizing the picture requires recalibration.

## 4. PS Ownership And Concurrency

`g26_hmi_task` is the sole UARTLite owner. It parses touch events, publishes
display commands, and never accesses a hard-coded hardware address.

The measurement task receives generation-tagged requests through a FreeRTOS
queue. Results are staged, published under a mutex, and returned through a
completion queue. The HMI task accepts only a completion matching the active
generation and source page. Repeated START events are ignored while a
generation is pending. STOP cancels publication for that HMI session; it does
not reset an in-progress DMA transfer.

## 5. Rebuild From A Clean Checkout

Use explicit output directories outside the repository. The first command is
required because generated Vivado wrappers are intentionally not tracked.

```powershell
& 'D:\Vivado_install\Vivado\2020.2\bin\vivado.bat' `
  -mode batch -nojournal -nolog `
  -source 25G_PL/script/pl_integrate_hmi_uart.tcl

$env:PL_HMI_UART_CANDIDATE_DIR = 'D:\path\to\new_pl_candidate'
& 'D:\Vivado_install\Vivado\2020.2\bin\vivado.bat' `
  -mode batch -nojournal -nolog `
  -source 25G_PL/script/pl_build_hmi_uart_candidate.tcl

$env:PS_HMI_CANDIDATE_XSA = `
  'D:\path\to\new_pl_candidate\25g_2026g_hmi_j11.xsa'
$env:PS_HMI_CANDIDATE_WORKSPACE = 'D:\path\to\new_vitis_workspace'
& 'D:\Vivado_install\Vitis\2020.2\bin\xsct.bat' `
  25G_PS/script/ps_build_hmi_candidate.tcl
```

Always pass the candidate XSA explicitly. The repository's historical
`25G_PL/top.xsa` is not this candidate. These commands do not invoke Bootgen
and do not create `BOOT.BIN`.

For a JTAG-assisted board run, set all four inputs explicitly and use the
matching candidate files only:

```powershell
$env:PS_HMI_CANDIDATE_BIT = 'D:\path\to\25g_2026g_hmi_j11.bit'
$env:PS_HMI_CANDIDATE_XSA = 'D:\path\to\25g_2026g_hmi_j11.xsa'
$env:PS_HMI_CANDIDATE_PS7_INIT = 'D:\path\to\ps7_init.tcl'
$env:PS_HMI_CANDIDATE_ELF = 'D:\path\to\hmi_candidate_app.elf'
& 'D:\Vivado_install\Vitis\2020.2\bin\xsct.bat' `
  25G_PS/script/ps_run_hmi_candidate.tcl
```

Capture PS UART1 at 115200 8N1. A successful touch exchange reports
`[HMI] EVENT`; a completed initial page update reports
`[HMI] DISPLAY_TX_COMPLETE`. The latter measures touch reception through the
last UARTLite byte leaving the FPGA. Physical rendering must still be observed
on the TJC and timed as part of board acceptance.

Before power-on, cross J11 TX to display RX and connect the level-reduced
display TX to J11 RX. Connect J11 pin 1 and display ground. Keep the adapter
USB disconnected unless its USB-UART isolation is proven. The screen is
powered from the system 5 V distribution, not from J11.

The portable BD hierarchy can be checked independently with:

```powershell
& 'D:\Vivado_install\Vivado\2020.2\bin\vivado.bat' `
  -mode batch -nojournal -nolog `
  -source 25G_PL/script/pl_validate_hmi_uart_bd.tcl
```

The portable PL source is organized like the teammate project under
`25G_PL/ip_core/pl_hmi_uart/{src,usage}`. The old
`25G_PL/script/pl_hmi_uart_bd.tcl` is only a compatibility entry point and
loads the source from `ip_core`; it is not a second implementation.

## 6. Current Verification Boundary

The reproducible offline candidate was built from source commit
`5b6ecdf27767cbd1aafda96ecba92069a024c922` with Vivado/Vitis 2020.2:

- bitstream `25g_2026g_hmi_j11.bit`: 4,045,663 bytes, SHA-256
  `4DD2EB78924B75C59FCCCE057F498259DB5313661C59232B461AE3EAE501ADEF`;
- XSA `25g_2026g_hmi_j11.xsa`: 841,150 bytes, SHA-256
  `1F848289D326ED229101076766660BC64DD8E76E406ECA18B022ECE408A3C14C`;
- the bit embedded in the XSA matches the standalone bitstream exactly;
- ELF `hmi_candidate_app.elf`: 2,203,076 bytes, SHA-256
  `7C4B8BE03253911F571FECECB91F530316143D5942A81F32742D9AD430748D16`;
- matching `ps7_init.tcl`: 34,511 bytes, SHA-256
  `3C4A9FF99EC83AB9C4A8CB98B21C3952AF484E809E050AA075FB151642BCB74D`;
- implementation WNS `+0.752 ns`, WHS `+0.035 ns`, DRC 0 errors,
  117 warnings and 2 advisories; minimum bus-skew slack `+8.337 ns`;
- standalone BD validation, active integration, full PL build, and Vitis build
  logs contain 0 Critical Warning and 0 Error;
- UART CDC contributes one `CDC-3 Info` RX synchronizer entry. The inherited
  design still has non-UART CDC critical findings and is not a clean CDC
  result;
- host HMI parser/render/session regression passed, and the Vitis application
  compiled and linked successfully.

This evidence proves offline build/implementation only. An earlier J11
UARTLite transport candidate passed one 1024-byte F17-to-F16 physical loopback,
but that result does not board-verify these final bit/XSA/ELF files or the TJC.
The final candidate still needs direct TJC communication, drawing correctness,
the required amplitude accuracy, and the less-than-2-second
start-to-final-display test. Measure that full interval during the first screen
test; 601 individual `add` commands consume about 0.73 s of line time at
115200 before display-side parsing and rendering are included.
