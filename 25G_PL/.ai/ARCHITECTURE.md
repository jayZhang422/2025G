# 2023H Vivado Project Architecture

This is the baseline knowledge base for the 2023H Vivado project. Future changes must update this document incrementally and retain established facts.

## 1. Project Overview

### Purpose and Platform

This is a Zynq-7020 PS+PL acquisition design targeting `xc7z020clg400-2`. PL samples a 12-bit AD9226 input and sends framed samples to PS DDR through AXI Stream and AXI DMA. The internal A/B DDS/DAC hierarchy is retained, but the active PL HMI UART candidate no longer exports physical DAC pins because J11 F16/F17 now carry UART RX/TX. The 2026-07-18 DDS corrections passed the active Vivado 2020.2 XSIM behavioral regression; DAC board validation is not applicable to the current top-level boundary.

The application-side PS source code is not in this Vivado workspace. Hardware exposes two PS contracts: a DMA destination in DDR and a ten-word BRAM control block for the DDS.

### System Composition

| Area | Current implementation |
| --- | --- |
| ADC | AD9226, 12-bit parallel input, 5.1208 MHz sampling |
| Acquisition | independent-clock FIFO, AXIS FIFO, AXI DMA S2MM, PS HP0 DDR |
| DDS/DAC | Two 32-bit DDS states and four ROM instances remain internal; physical DAC outputs are open at `top.v` in the UART candidate |
| PS | PS7 DDR, FCLK0 100 MHz, M_AXI_GP0, S_AXI_HP0, UART1 MIO 48/49, SD0 MIO40..45/CD47, three EMIO GPIO inputs |
| HMI transport | AXI UARTLite 115200 8N1 on FCLK0; RX F16/J11 pin 4, TX F17/J11 pin 3; polled first integration |
| Debug | `fifo_monitor_axi_0` publishes FIFO/AXIS snapshots to PS through AXI4-Lite; `ila_0` remains uninstantiated |
| Tooling | Vivado 2020.2, Questa is the selected target simulator |

PL owns sampling, FIFO CDC, AXIS framing, DDS/ROM internals, and the PL HMI UART pins. PS owns DMA configuration, DDR, DDS control BRAM writes, and polled UARTLite access. The physical DAC boundary is retired without renaming the established `H_top` DAC interfaces.

## 2. Top-Level Structure

Synthesis top is `top`; active simulation top is `tb_H_top`. `system_wrapper` is generated from the BD and instantiates one `system` module.

```text
top
|- H_top u_h_top
|  |- PLL_AD ad_clk                             (Clocking Wizard)
|  |- ad_fifo_wrapper_0 u_ad_fifo               (local user IP)
|  |  |- ad9226 u_ad9226
|  |  `- fifo -> fifo_generator_0 my_fifo_ip
|  `- DDS_DAC u_dac_dds                         (local user IP)
`- system_wrapper u_system                      (generated from system.bd)
   `- system system_i
      |- processing_system7_0                   (Zynq PS7)
      |- proc_sys_reset_0
      |- Pll_DA                                 (125 MHz Clocking Wizard)
      |- axi_dma_adc                            (S2MM DMA)
      |- axis_data_fifo_0
      |- smartconnect_0
      |- axi_interconnect_0 -> xbar, auto_pc
      |- pl_hmi_uart_0 -> axi_uartlite_core
      |- axi_bram_ctrl_0
      `- blk_PS_TO_PL                           (true dual-port BRAM)
```

`ram.sv`, `top_tb.sv`, `blk_mem_gen_0.xci`, and `ila_0.xci` are AutoDisabled in `2023H.xpr`; they are retained project assets but are not active hierarchy.

## 3. Data Flow

### ADC and FIFO

```text
i_ad_data[11:0]
 -> ad9226 capture on clk_pll_deg (5.1208 MHz, about 181.891 degrees)
 -> stable/valid after PLL lock plus 64 sample-clock cycles
 -> fifo_generator_0 write port
 -> {ADC[11:0], 4'b0} as 16-bit data
 -> FIFO read in FCLK0 domain
 -> H_top AXIS source
 -> ADC_STREAM_IN -> axis_data_fifo_0 -> axi_dma_adc S_AXIS_S2MM
 -> smartconnect_0 -> PS S_AXI_HP0 -> DDR
```

The first FIFO is the intended 5.12 MHz to 100 MHz CDC boundary. `fifo.v` blocks writes on `prog_full` and reads on `prog_empty`/reset-busy. `H_top` emits `TLAST` on every 4096th accepted AXIS beat. Its counter, `tvalid`, `tready`, and `tlast` are all in the 100 MHz FCLK0 domain.

BD `fifo_monitor_axi_0` observes this path without feeding any signal back into
it. PS writes AXI4-Lite control bits to request a snapshot or clear sticky faults,
then reads the published counters at `0x43C1_0000`. The monitor ignores FIFO
`full` while `wr_rst_busy` is active because the FIFO Generator intentionally
resets its full flag to one. Its AXIS counters describe acceptance into the BD
AXIS FIFO, not completion of the downstream DMA write.

The second stage is BD `axis_data_fifo_0`: depth 4096, 16-bit AXIS and TLAST. It is configured asynchronous even though both its ports currently use FCLK0.

### DDS, ROM, and DAC

```text
PS M_AXI_GP0 -> AXI Interconnect -> AXI BRAM Controller Port A
                                  -> blk_PS_TO_PL true dual-port BRAM
                                       Port B -> H_top BRAM_DATA -> ad9767

ad9767 BRAM poll -> A/B shadow registers -> COMMIT_SEQ apply event
                  -> independent phase_acc_a / phase_acc_b
                  -> A/B sine and triangle ROMs
                  -> independent waveform select, scale, and saturation
                  -> da_data_a / da_data_b
```

`ad9767` polls ten 32-bit words continuously, one selection per DAC clock:

The active UART candidate keeps this internal hierarchy for compatibility but
leaves all `H_top` DAC outputs open in `top.v`; no DAC package pin is driven.

| Offset | Field | Behavior |
| --- | --- | --- |
| `0x00` | A waveform flag bit 0 | 0 sine, 1 triangle |
| `0x04` | A phase step | increment added each 125 MHz DAC clock |
| `0x08` | A absolute phase | used by a phase-reload commit |
| `0x0C` | A amplitude `[13:0]` | scales centered ROM code |
| `0x10` | B waveform flag bit 0 | 0 sine, 1 triangle |
| `0x14` | B phase step | increment added each 125 MHz DAC clock |
| `0x18` | B absolute phase | used by a phase-reload commit |
| `0x1C` | B amplitude `[13:0]` | scales centered ROM code |
| `0x20` | control | bit 0 RUN, bit 1 phase reload, bit 2 live B phase delta |
| `0x24` | commit sequence | written last by PS to request an atomic apply |

ROM values are unsigned and centered around 8192 before multiplication. DAC data saturates to 0 through 16383. `da_wrt_a` and `da_wrt_b` are `~clk_dac`. A/B DAC data are calculated independently. The source resets all shadow registers, commits RUN only through `COMMIT_SEQ`, accepts a stopped commit, and forces midscale while stopped. A changed sequence is held pending through a complete shadow-register scan before it is applied, preventing a partially refreshed snapshot. These behaviors passed behavioral simulation.

For control bit 2, `0x18` is interpreted as a signed B-channel phase delta.
The apply edge advances both channels by their current phase steps, then adds
the delta only to B. This is distinct from bit 1, which reloads both absolute
phase accumulators.

### AXI, DMA, PS, and ILA

The BD external AXIS port `ADC_STREAM_IN` is a 16-bit input to the BD from PL; it has TREADY and TLAST, but no TKEEP/TSTRB/TID/TDEST/TUSER. DMA is simple mode S2MM only: no MM2S and no scatter-gather. Its stream width is 16 bits and memory AXI width is 64 bits. PS writes its AXI-Lite registers and DMA transfers to DDR through HP0.

`ila_0` is uninstantiated. Its saved configuration has six probes of widths 12, 1, 1, 12, 12, and 14; depth is 4096 with advanced trigger and storage qualification enabled.

## 4. Clock System and CDC

| Clock | Source | Frequency | Active users |
| --- | --- | ---: | --- |
| `i_clk_50m` | pin U18 | 50 MHz | `PLL_AD` input |
| `clk_pll_out` | PLL_AD output 1 | 65 MHz | exported by `ad9226`, unused otherwise |
| `clk_pll_ad` / `o_ad_clk` | PLL_AD output 2, 0 deg | 5.1208 MHz | physical ADC clock only |
| `clk_pll_deg` | PLL_AD output 3, requested 181.8 deg | 5.1208 MHz | ADC capture and FIFO write |
| `FCLK_CLK0_0` / `clk_fpga_0` | PS7 | 100 MHz | FIFO read, AXIS, DMA, AXI control |
| `clk_dac` | BD Pll_DA | 125 MHz | DDS, ROMs, DAC, BRAM Port B |

`PLL_AD` Clocking Wizard 6.0 converts 50 MHz to 65 MHz plus the two 5.12 MHz phases. BD `Pll_DA` Clocking Wizard 6.0 converts FCLK0 to 125 MHz.

The XDC cuts paths both ways between `clk_pll_deg_PLL_AD` and `clk_fpga_0` for the independent-clock FIFO. No false path is used for the former `clk_pll_ad` to FCLK0 AXIS error; that error was removed structurally by clocking the TLAST counter with FCLK0.

Current post-route timing: WNS 0.398 ns, TNS 0, WHS 0.009 ns, THS 0. All user timing constraints are met; the Inter-Clock table is empty.

## 5. Reset System

| Reset | Polarity and source | Propagation |
| --- | --- | --- |
| `i_rst` | active low top-level input | AD capture, DDS/DAC, TLAST counter, BRAM Port-B reset inversion |
| `w_rst_safe` | active low, `i_rst & w_ad_valid` | FIFO wrapper input, holds FIFO until ADC is valid |
| `rst_fifo_n` | active-high release flag in FIFO write domain | after 15 write clocks, drives FIFO Generator reset as `~rst_fifo_n` |
| `FCLK_RESET0_N` | active low PS7 output | `proc_sys_reset_0.ext_reset_in` and `Pll_DA.resetn` |
| `peripheral_aresetn` | active low reset block output | DMA, AXIS FIFO S side, interconnect, SmartConnect, BRAM Controller, UARTLite |

Custom RTL uses asynchronous active-low resets. FIFO IP has internal CDC reset handling. BRAM Port B reset is derived from `i_rst`, not the PS peripheral reset.

## 6. Block Design

The PS7 EMIO GPIO input is enabled at width 3 and exported as `pl_key_i[2:0]`.
The XDC maps bit 0/1/2 to N16/T17/R17 respectively; software reads them as
active-low XGpioPs pins 54/55/56.

| BD instance | Function | Important connections |
| --- | --- | --- |
| `processing_system7_0` | PS7, DDR, FCLK, AXI | FCLK0, M_AXI_GP0, S_AXI_HP0, UART1 |
| `proc_sys_reset_0` | FCLK reset fanout | creates peripheral active-low reset |
| `Pll_DA` | 100 MHz to 125 MHz | output is `clk_dac` |
| `axi_dma_adc` | simple S2MM DMA | AXIS FIFO to SmartConnect/HP0 |
| `axis_data_fifo_0` | AXIS FIFO | `ADC_STREAM_IN` to DMA S2MM |
| `smartconnect_0` | DMA memory path | S00 from DMA; M00 to HP0; M01 to PS S_AXI_GP0 |
| `axi_interconnect_0` | PS control fanout | GP0 to DMA, BRAM, IQ, monitor, DDC, and UARTLite (`M05_AXI`) |
| `xbar`, `auto_pc` | AXI interconnect internals | generated support cells |
| `axi_bram_ctrl_0` | AXI4-Lite to BRAM | Port A of `blk_PS_TO_PL` |
| `blk_PS_TO_PL` | true dual-port BRAM | Port A PS; Port B DDS |
| `pl_hmi_uart_0` | AXI UARTLite hierarchy | 115200 8N1 at `0x43C3_0000`; interrupt deliberately unconnected |

Generated child BD `bd_919a` implements SmartConnect internals: AXI-to-SC and SC-to-AXI adapters, entry/exit pipelines, nodes, switchboards, clock map, and two MI paths. It is a generated implementation detail, not a second user BD.

### Address Map

| PS space | Target | Base | Range |
| --- | --- | ---: | ---: |
| `processing_system7_0/Data` | BRAM Controller memory | `0x4000_0000` | 8 KiB |
| `processing_system7_0/Data` | DMA AXI-Lite registers | `0x4040_0000` | 64 KiB |
| `processing_system7_0/Data` | IQ demodulator registers | `0x43C0_0000` | 64 KiB |
| `processing_system7_0/Data` | FIFO monitor AXI-Lite registers | `0x43C1_0000` | 64 KiB |
| `processing_system7_0/Data` | DDC stream registers | `0x43C2_0000` | 4 KiB |
| `processing_system7_0/Data` | PL HMI UARTLite registers | `0x43C3_0000` | 64 KiB |
| `axi_dma_adc/Data_S2MM` | PS HP0 DDR/OCM | `0x0000_0000` | 1 GiB |

DMA mappings to GP0 IOP (`0xE0000000`, 4 MiB) and GP0 master space (`0x40000000`, 1 GiB) are explicitly excluded. DMA S2MM interrupt remains on `xlconcat_0/In0`, IQ remains on `In1`, and the concat output drives PS `IRQ_F2P`.

## 7. IP Inventory

Configure source IPs through Vivado IP customization or BD editing, then regenerate output products. Never hand-edit XCI/cache/generated output files.

| Source XCI | IP and key parameters | Status/dependency |
| --- | --- | --- |
| `PLL_AD.xci` | clk_wiz 6.0; 50 -> 65, 5.12@0 deg, 5.12@181.8 deg | active in `ad9226` |
| `fifo_generator_0.xci` | FIFO Generator 13.2; native 16-bit, independent-clock BRAM | active in `fifo` |
| `blk_rom_sine.xci` | blk_mem_gen 8.4; single-port 4096x14 ROM | active, sine COE |
| `blk_rom_triangle.xci` | blk_mem_gen 8.4; single-port 4096x14 ROM | active, triangle COE |
| `blk_mem_gen_0.xci` | blk_mem_gen 8.4; simple dual-port 4096x16 RAM | disabled, `ram.sv` only |
| `ila_0.xci` | ILA 6.2; six probes, depth 4096 | disabled/uninstantiated |
| `system_processing_system7_0_0.xci` | processing_system7 5.5; FCLK0=100 MHz, GP0/HP0/UART1 enabled | active BD PS |
| `system_proc_sys_reset_0_0.xci` | proc_sys_reset 5.0 | active reset fabric |
| `system_clk_wiz_0_0.xci` | clk_wiz 6.0; 125 MHz output | active Pll_DA |
| `system_axi_dma_0_0.xci` | axi_dma 7.1; S2MM, 16-bit AXIS, 64-bit memory, no SG | active BD |
| `system_axis_data_fifo_0_0.xci` | axis_data_fifo 2.0; depth 4096, TLAST, async, 16-bit | active BD |
| `system_smartconnect_0_0.xci` | smartconnect 1.0; two MI | active BD |
| `system_axi_interconnect_0_0.xci` | axi_interconnect 2.1; 2 SI/6 MI | active control plane |
| `system_xbar_0.xci` | axi_crossbar 2.1; 32-bit AXI-Lite, 2 SI/6 MI | active child |
| `system_auto_pc_0.xci` | axi_protocol_converter 2.1; 32-bit | active child |
| `system_axi_uartlite_core_0.xci` | axi_uartlite 2.0; 115200, 8 data bits, no parity, 100 MHz AXI clock | active under `pl_hmi_uart_0` |
| `system_axi_bram_ctrl_0_0.xci` | axi_bram_ctrl 4.1; AXI4-Lite, single-port, 32-bit, latency 1 | active BD |
| `system_blk_mem_gen_0_0.xci` | blk_mem_gen 8.4; true dual-port 2048x32, Port B 125 MHz | active control BRAM |

The workspace contains 101 physical XCI files: 17 source configurations, 49 cache configurations, and 35 generated SmartConnect/support configurations. The latter two groups are derived duplicates or sub-IP dependencies, not independent user-maintained IP definitions.

## 8. RTL Inventory

| File / module | Role, state, dependencies, standalone simulation |
| --- | --- |
| `top.v` / `top` | synthesis top; physical ADC, PL HMI UART, DDR, and PS pins; instantiates `H_top` and generated wrapper; leaves the established internal DAC outputs open |
| `H_top.v` / `H_top` | PL integration; `ad_fifo_wrapper_0`, AXIS, BRAM, and `DDS_DAC` wiring plus 12-bit TLAST counter; simulated by `tb_H_top` |
| `ad9226.v` / `ad9226` | AD capture/clock wrapper; 7-bit 64-cycle stabilization counter and `stable` flag; depends on `PLL_AD` |
| `fifo.v` / `fifo` | 5.12 MHz write to 100 MHz read wrapper; 4-bit reset counter; depends on `fifo_generator_0` |
| `ad9767.sv` / `ad9767` | dual DDS/DAC driver; ten-word BRAM polling, A/B shadow/running registers, two phase accumulators, four ROM instances, and independent DAC registers; source is under review |
| `ram.sv` / `ram` | disabled 16-bit ping-pong two-BRAM buffer; parameters `DATA_WIDTH=16`, `FFT_LENGTH=4096`; depends on `blk_mem_gen_0` |

## 9. Testbench

`tb_H_top.v` is active. It generates 50/100/125 MHz clocks, reset, incrementing 12-bit ADC input, and permanently-ready AXIS sink. A ten-word array emulates synchronous BRAM Port B. It checks deterministic reset, uncommitted-shadow isolation, atomic A/B startup, continuous A/B phase advance, independent DAC values, uncommitted RUN isolation, tracking commits at the actual apply edge, 0 to 180 degree sine/sine phase reloads in 5 degree increments, and a stopped midscale output. The Vivado 2020.2 XSIM behavioral regression passed all five stages at 7385 ns. A full-time-axis view intentionally contains reload discontinuities during the phase sweep and ends at midscale after the final STOP commit; use the stage-two/three windows, not the whole TB run, to inspect steady waveform shape.

It does not model PS7, AXI-Lite, DMA, DDR, FIFO back-pressure/full behavior, or a 4096-beat TLAST frame. `top_tb.sv` is disabled, uses only a subset of the current `H_top` ports, and is not a valid regression bench for the active interface.

## 10. ROM and COE Files

| File | Format and use |
| --- | --- |
| `sine_wave_14bit_4096.coe` | radix 10, 4096 unsigned entries, 0..16383, starts 8192/8204/...; initializes sine ROM |
| `triangle_wave_14bit_4096.coe` | radix 10, 4096 unsigned entries, 0..16383, starts 0/8/...; initializes triangle ROM |

Both have 12-bit addresses through `phase_acc[31:20]`. XCI output products generate corresponding MIF files for simulation.

## 11. Tooling and Dependencies

- Vivado 2020.2 (build 3064766), device 7z020-clg400 speed grade -2.
- Verilog-2001 and SystemVerilog coexist. Preserve each file's existing style; `ad9767.sv` and `ram.sv` use SV constructs. See `.ai/STYLE.md`.
- `system.tcl` rebuilds the BD and requires Vivado 2020.2.
- Forty additional Tcl files are generated PS-init, simulation, synthesis, implementation, and waveform-control scripts.
- `2023H.gen`, `2023H.cache`, `2023H.ip_user_files`, `2023H.runs`, and `2023H.sim` are generated output products; regenerate them, do not edit them.

## 12. Current Risks

1. The physical DAC timing constraints are retired with the top-level DAC
   ports. AD9226 input timing remains unverified because the available module
   manual does not provide a complete output maximum/hold specification.
2. `i_rst` is asynchronously deasserted in several custom clock domains; `w_rst_safe` originates in ADC domain and resets the FIFO wrapper. Review reset deassertion when changing clocks or FIFO settings.
3. The ten-word shadow/commit protocol passes the one-clock BRAM behavioral model, but it still needs hardware validation against the generated true-dual-port BRAM and PS AXI writes.
4. FIFO/AXIS counters and sticky status are PS-readable through the passive
   monitor, but the inherited full-design CDC report still requires review.
5. AXIS FIFO asynchronous configuration is redundant with its present common FCLK0 ports and should be explicitly justified or revised.
6. DMA and IQ fabric IRQs are wired. UARTLite is deliberately not in the IRQ
   concat and must be polled during first bring-up.
7. Active simulation does not prove DMA/DDR, TLAST cadence, FIFO stress, or reset release across all clocks.
8. Disabled RAM, ILA, and legacy testbench assets create maintenance ambiguity.
9. The dual DDS source passed XSIM behavioral simulation only. The test does not prove board-level DAC timing, analogue amplitude calibration, or PS AXI write ordering under real hardware load.

## 13. TODO

1. Add converter- and board-derived input/output delay constraints.
2. Run and inspect the ten-word atomic DDS shadow-register and commit/version protocol in Questa and on hardware.
3. Decide whether BD AXIS FIFO must remain asynchronous.
4. Keep UARTLite polling for first bring-up; evaluate an interrupt/ring buffer
   only after physical loopback and TJC burst behavior are measured.
5. Add test coverage for 4096-beat TLAST, back-pressure, FIFO overflow, reset, stop/midscale, independent A/B waveform and amplitude, phase reload, phase-continuous tracking, triangle mode, and DMA/DDR integration.
6. Decide whether disabled RAM, ILA, and legacy testbench are retained or removed after deliberate review.
7. Integrate PS control software only after board-level verification of the dual-DDS outputs.

## Source Coverage Record

This initialization read source RTL, both testbenches, all 101 physical XCI files including source/cache/generated dependencies, all three BD files, all 41 Tcl files, both COE files, XDC, XPR, generated system wrapper, existing Markdown, and current post-route timing/DRC reports. Binary checkpoints, bitstreams, simulator databases, and vendor HDL were inventoried as generated artifacts rather than treated as authored RTL.

<!-- SYNC: PL_AUTOMATION_CONTRACT_START -->
## 14. PL Automation Contract

The authoritative PL automation entry points are in `2023H_PL/script` and
use Vivado 2020.2 with project `2023H_pl.xpr`.

- `pl_update_bd.tcl` reuses an already-open `2023H_pl` project, opens it only
  when none is open, and refuses to operate when another project is open. It
  runs `validate_bd_design` (Vivado F6) before saving the BD, regenerating
  Output Products, regenerating `system_wrapper.v`, or exporting `system.tcl`.
  A validation failure stops the workflow before those writes. Output Products
  and the wrapper use `-force` so a validated BD update is not skipped merely
  because prior generated files exist.
- `pl_build_bitstream.tcl` defaults to `general.maxThreads=24` and `-jobs 24`.
  It considers hardware current only when `top.bit` exists, `synth_1` is
  `synth_design Complete!` with `NEEDS_REFRESH=0`, and `impl_1` is
  `write_bitstream Complete!` with `NEEDS_REFRESH=0`. If current, it prompts
  for `1` to reset and fully rebuild or `0` to keep `top.bit` and `top.xsa`.
  `--rebuild` and `--keep` provide noninteractive equivalents; an out-of-date
  design always rebuilds. A rebuild resets `impl_1`, resets `synth_1`, runs and
  waits for synthesis, then runs implementation through bitstream generation
  before exporting the XSA with the bitstream.
- `pl_update_bd.bat` and `pl_build_bitstream.bat` are double-click Windows
  launchers. They call their paired TCL files, forward optional arguments, and
  keep the console open to display progress or errors.
- `--check` validates object names and existing generated files without
  regenerating outputs. `pl_update_bd.tcl --validate-only` performs only the
  F6-equivalent BD validation.

Script validation in this session passed for the existing `2023H_pl` project,
including the already-open-project path, BD validation, bitstream current-state
detection, and the interactive `0` / `--keep` no-write path. `system.tcl`,
`top.xsa`, `top.bit`, and `2023H_pl.xpr` were not modified during these checks.
<!-- SYNC: PL_AUTOMATION_CONTRACT_END -->

## 15. IQ Integration Status

The active IQ data path is now:

```text
AD9226 registered sample and stable valid
 -> ad_fifo_wrapper_0 adc_raw/sample_valid
 -> H_top IQ outputs
 -> top
 -> generated system_wrapper
 -> iq_demodulator_0 in system.bd
 -> AXI-Lite result/control registers and IRQ_F2P
```

`iq_demodulator_0` is controlled from `0x43C0_0000`. It runs its DSP and DDS
in the ADC phase-clock domain, while its AXI-Lite control/status interface
runs on FCLK0. Its configuration and result paths include CDC handshakes;
the top-level must not substitute raw ADC pins or FCLK0 for the four IQ
wrapper ports. See `.ai/ip_rep.md` for the maintained port and IP inventory.

## 16. PS/PL Architecture Synchronization

The maintained cross-system record is `../../25G_系统当前架构.md`; the PS-side
software view is `../../25G_PS/.ai/architecture.md`. The active IQ capability is
**IQ demodulation**, not IQ modulation: ADC-domain registered samples are mixed
with an ADC-domain DDS sin/cos pair and accumulated into I/Q windows. The PS only
configures and reads this IP through AXI-Lite. The existing ADC-to-DMA-to-PS FFT
path remains active for unknown-frequency search and waveform classification.

The DAC path is separately implemented by `DDS_DAC`: it uses a ten-word BRAM
snapshot and independent single-tone DDS states. Since it has no streaming I/Q
baseband input, interpolation, pulse shaping, or quadrature up-converter, it is
not an IQ modulator. IP-specific use documents are maintained in
`ip_core/*/usage/README.md`.

## 17. PL HMI UART Integration Candidate

The active `calvin-uart-integration` candidate adds
`pl_hmi_uart_0/axi_uartlite_core` as `axi_interconnect_0/M05_AXI`. Its AXI
clock is PS FCLK0 at 100 MHz and its active-low reset is
`proc_sys_reset_0/peripheral_aresetn`. The serial interface is fixed at 115200
baud, 8 data bits, no parity, and one stop bit. UART interrupt remains
unconnected, so DMA stays on `xlconcat_0/In0`, IQ stays on `In1`, and the
concat output remains the only connection to `IRQ_F2P`.

The generated wrapper ports are `PL_HMI_UART_rxd` and `PL_HMI_UART_txd`.
`top.v` maps them without renaming to `i_hmi_uart_rx` and
`o_hmi_uart_tx`. Active constraints map RX to J11 pin 4/F16 and TX to J11
pin 3/F17, both LVCMOS33. Post-place `report_io` confirms those assignments.

Vivado 2020.2 implementation passed with WNS `+3.358 ns`, WHS `+0.036 ns`,
zero DRC errors, and minimum bus-skew slack `+8.352 ns`. The full inherited
design still has 147 DRC warnings and CDC Critical findings outside UARTLite.
The only UART CDC entry is `CDC-3 Info` for the core's RX double-register
synchronizer. Physical F17-to-F16 loopback and TJC tests remain pending.
