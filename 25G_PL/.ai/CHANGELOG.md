# Change Log

## 2026-07-31 - 2026 G Decimating FIR Active Contract And Board Validation

### Current Architecture

- Recorded the active `H_top -> adc_fir_axis -> system_wrapper` input path.
- `adc_fir_axis` centers the 12-bit AD9226 samples, drives `fir_compiler_0`,
  rounds and saturates the Q1.17 result to `signed16`, and emits `TLAST` every
  4096 accepted decimated outputs.
- The 39-tap FIR runs at `5,120,060 Hz`, decimates by 3, has a 0..500 kHz
  passband, `-0.008071 dB` at 500 kHz, and `-67.630308 dB` at 1 MHz.
- The SG DMA contract remains 4096 `signed16` samples and 8192 byte per frame.
- IQ/DDC and DDS/DAC hardware remain packaged but are inactive in the current
  G26 PS application.

### Board Evidence

- PS board measurements using the FIR output are stable across 10..500 kHz.
- A passive external low-pass contributes about 14 dB attenuation at 1 MHz;
  together with the FIR, typical 1 MHz cascade attenuation is about 81.6 dB.
- With 250/500 kHz valid components, adding 200 mVpp interference at 1, 1.5,
  and 2 MHz changed the reported valid parameters by less than 0.2 mV.
- The `4.62006..5.62006 MHz` raw-ADC alias window is not yet validated and
  cannot be delegated to the digital FIR after ADC aliasing.

## 2026-07-24 - Windows-Compatible FIFO Monitor Driver Packaging

### Fixed

- Replaced `*.c`, `*.h`, and `*.o` in the packaged
  `ad_fifo_monitor_axi_v1_0` driver Makefile with explicit source, header, and
  object lists. Vitis 2020.2 invokes custom drivers with `SHELL=CMD`, where the
  wildcard object passed to `arm-none-eabi-ar` and `rm` was not expanded.
- Updated the packaged IP checksums and upgraded the active BD instance without
  changing its interface or parameters.

### Verification

- A full Vivado synthesis, implementation, and bitstream rebuild completed
  with zero DRC errors and exported a non-stale XSA.
- The exported XSA contains the corrected driver Makefile, the IP instance is
  unlocked, and Vitis rebuilt both FSBL and FreeRTOS BSP libraries successfully.

## 2026-07-24 - FIFO Transparency And Monitor Integration

### Changed

- Replaced the obsolete `ad_fifo_output` top-level instance with
  `ad_fifo_wrapper_0` while preserving the validated FIFO Generator settings,
  programmable thresholds, FWFT mode, reset extension, and read/write gates.
- Captured `din` on the ADC stabilization transition so the first asserted
  `sample_valid` leads to a real registered sample on the first FIFO write.
- Replaced the standalone `ad_fifo_monitor_0` with BD `fifo_monitor_axi_0`.
  PS now requests snapshots, clears sticky flags, and reads the monitor counters
  through AXI4-Lite at `0x43C1_0000`.
- Prevented `fifo_full_sticky` from latching the FIFO Generator's intentional
  reset value by ignoring `fifo_full` while `fifo_wr_rst_busy` is asserted.
- Declared the ADC/write clocks as 5,120,060 Hz IP clock interfaces, associated
  both custom-IP clock domains with `rst_n`, and moved the project IP repository
  reference to the versioned `25G_PL/ip_core` directory.
- Updated `refresh_iq_ip_and_wrapper.tcl` to refresh both FIFO packages and use
  the Vivado 2020.2 `IPDEF` instance property.

### Verification

- Refreshed both user IPs, DDS output products, the validated Block Design, and
  generated `system_wrapper.v` without running simulation or synthesis.
- Vivado `check_syntax -fileset sources_1` reported no errors or warnings, and
  all project IP instances were unlocked.

## 2026-07-23 - IQ Demodulator Top-Level Integration

### Changed

- Repackaged local `ad_fifo_output` with `adc_raw[11:0]` and
  `sample_valid` outputs. Both mirror the registered AD9226 sample and the
  post-stabilization valid flag already consumed by the FIFO write port; the
  DMA acquisition path is unchanged.
- Set `25G_PL/ip_core` as the project IP repository and upgraded the active
  `ad_fifo_output` instance from that local package.
- Corrected `dds_iq_lo` AXI clock metadata to `5,120,060 Hz`. Its DDS clock
  rate remains `5.12006 MHz`, while phase increment and phase offset remain
  runtime-programmable through the IQ IP AXI-Lite interface.
- Replaced the stale imported `system_wrapper.v` source with the current BD
  generated wrapper. The old file remains on disk only and is no longer in
  `sources_1`.
- Connected the IQ BD ports in `top.v`: the ADC phase clock, registered raw
  sample, valid flag, and active-low reset now flow from `H_top` through the
  generated wrapper to `iq_demodulator_0`.
- Added `.ai/ip_rep.md` as the current IP inventory and interface-maintenance
  reference.

### Verification

- Local custom-IP packaging, `upgrade_ip`, and generated HDL refresh
  completed without launching simulation, synthesis, implementation, or
  bitstream generation.
- `validate_bd_design` completed successfully.
- Vivado 2020.2 `check_syntax -fileset sources_1` reported no errors or
  warnings.

## 2026-07-22 - Top-Level Custom-IP Integration

### Changed

- Replaced the direct `ad9226` plus `fifo` hierarchy in `H_top.v` with the
  added `ad_fifo_output` IP. `PLL_AD` remains at top level because the AD IP
  no longer owns the clock wizard: its 0-degree output still drives
  `o_ad_clk`, and its phase-shifted output still clocks ADC capture/FIFO
  writes.
- Replaced the direct `ad9767` hierarchy with the added `DDS_DAC` IP. The
  existing four independent sine/triangle ROM instances remain at top level
  and are connected to the new IP address/data ports on `i_clk_dac`.
- Preserved the 16-bit AXIS data path, 4096-beat TLAST counter, ten-word BRAM
  map, atomic COMMIT_SEQ behavior, ODDR-forwarded DAC CLK/WRT pins, and
  falling-edge DAC data registration. The new arbitrary-waveform ports are
  held at midscale because the existing PS contract selects only sine or
  triangle.
- Updated the active testbench's backdoor observation paths from the old
  `u_ad9767` instance to `u_dac_dds.inst`; test stimulus and assertions are
  unchanged.
- No PACKAGE_PIN or IOSTANDARD assignment changed. The XDC now references the
  renamed phase clock (`clk_pll_phase_PLL_AD`) and the new DDS IP hierarchy for
  DAC output-register IOB placement.

### Verification

- Vivado 2020.2 generated the added IP simulation targets successfully.
- The existing `tb_H_top` behavioral regression passed all seven stages in
  XSim and finished normally at 35246 ns. It covered atomic DDS commits,
  independent A/B output, tracking continuity, B phase adjustment, STOP
  midscale, and AXIS TLAST under backpressure.
- The same design compiled with the configured Questa flow without HDL errors;
  execution could not start because the installed ModelSim ASE lacks `vopt`.
- No synthesis, implementation, or bitstream generation was run.

### Required Follow-Up

- `PLL_AD` now produces 5.12006 MHz, while the prior design and PS sampling
  contract use 5.12080 MHz. Reconfigure the clock wizard to restore 5.12080
  MHz before hardware deployment, or update the PS sampling constant in a
  coordinated change.
- `DDS_DAC.xci` declares its `clk` interface as 100 MHz, but `H_top` connects
  it to the 125 MHz DAC clock. Repackage/configure the custom IP metadata as
  125 MHz before timing closure. This is not a physical-pin constraint change.

## 2026-07-19 - AD9767 Source-Synchronous Output Timing

### Changed

- Registered AD9767 data on the falling DAC-clock edge after a full-cycle DDS
  sample pipeline. Forwarded each A/B CLK and WRT pin through its own ODDR,
  keeping WRT and CLK in phase at the module pins.
- Added generated-clock and 2 ns setup / 1.5 ns hold output-delay constraints
  for DA/DB relative to both physical CLK and WRT outputs. Data registers are
  constrained into OLOGIC; ILA observes the preceding sample register so it
  does not block that packing.
- Added active-testbench assertions for pin-clock phase and data stability at
  the forwarded clock edge.

### Verification

- XSIM behavioral regression passed all six stages.
- Final routed timing met all user constraints with WNS 0.023 ns, WHS 0.021
  ns, and 1.525 ns worst DAC data-output setup slack.

## 2026-07-19 - Complete PS/PL Automation Record

### Scope

Recorded all PS/PL automation work completed in this conversation. No RTL,
constraint, BD content, XSA, bitstream, ELF, or generated platform output was
intentionally modified during script validation.

### PS Changes

- Added PS TCL flows for platform/XSA update plus BSP source regeneration,
  source-only system rebuild, and program/run. Added PS generated-output
  ignore rules and timestamped progress output.
- Corrected the BSP action to `bsp regenerate`; `bsp reload` reloads saved
  settings and is not the GUI Revert BSP Sources operation.

### PL Changes

- Added `../script/pl_update_bd.tcl` and `pl_update_bd.bat`. The Tcl reuses
  the open target project when possible, performs F6 BD validation before any
  output write, forces Output Product/wrapper regeneration after validation,
  and exports `system.tcl`.
- Added `../script/pl_build_bitstream.tcl` and `pl_build_bitstream.bat`. It
  defaults to 24 Vivado worker threads/job slots, checks current bitstream
  state, prompts for `1` full rebuild or `0` keep-current, and supports
  `--rebuild`, `--keep`, and `--threads N`.
- A full rebuild resets `impl_1`, resets `synth_1`, runs synthesis, then runs
  implementation through bitstream generation before exporting the XSA. This
  resolves the prior already-current `write_bitstream` launch error.
- Added PL generated-output ignore rules and the requested Git upload rule in
  PS and PL `AGENTS.md` files.

### Verification

- Verified in Vivado 2020.2: target project discovery, already-open reuse, F6
  BD validation, current hardware detection, `--check`, `--validate-only`,
  and interactive `0` / `--keep` no-write behavior.
- Verified in XSCT 2020.2: PS workspace, platform, system project, bitstream,
  XSA, PS initialization script, and ELF names in check mode.
- Removed temporary Vivado session artifacts after validation. Key generated
  hardware files retained their pre-check timestamps.

## 2026-07-19 - PL Automation Scripts

### Changed

- Added `../script/pl_update_bd.tcl` to regenerate `system.bd` output
  products, create/add `system_wrapper.v`, and export `system.tcl`.
- Added `../script/pl_build_bitstream.tcl` to run `impl_1` through
  `write_bitstream` and export `top.xsa` with the bitstream.
- Added `../.gitignore` rules for Vivado-generated products and local session
  data.
- Added the user-requested Git upload rule to `AGENTS.md`. The PL directory is
  not currently a Git repository, so it cannot be committed or pushed until a
  repository and remote are initialized.
- Set the PL build default to 24 Vivado worker threads and 24 run-job slots,
  with `--threads <N>` available as an explicit override. Added timestamped
  progress output to both PL scripts.
- The `--check --threads <N>` path now validates `general.maxThreads` in the
  running Vivado version without launching synthesis or implementation.
- PL TCL scripts now reuse an already-open `2023H_pl` project and close the
  project only when they opened it themselves. They stop with an explicit
  error when a different project is open.
- `pl_update_bd.tcl` now runs `validate_bd_design` (the Vivado F6 action)
  before any save, Output Product generation, HDL wrapper creation, or BD Tcl
  export. Validation failure stops the flow without writing those outputs.
- Added `pl_update_bd.bat` and `pl_build_bitstream.bat` for double-click
  Windows execution; each invokes its matching TCL and leaves the console open
  so progress and failure messages remain visible.

## 2026-07-18 - Dual-DDS Source Review and Documentation Update

### Scope

- User independently modified `2023H.srcs/sources_1/new/ad9767.sv`.
- User independently modified `2023H.srcs/sim_1/new/tb_H_top.v`.
- No HDL, IP, block-design, constraint, or PS source file was modified during this review.
- `.ai/ARCHITECTURE.md` was updated as the intended target of the requested but nonexistent `.ai/agriculture.md` path.

### Source Changes Observed

- The DDS control BRAM map was expanded from four to ten 32-bit words: independent A/B waveform, phase step, initial phase, and amplitude words plus `RUN`, `PHASE_RELOAD`, and `COMMIT_SEQ`.
- `ad9767.sv` now contains independent A/B phase accumulators, A/B running control registers, A/B scale/saturation paths, and two sine plus two triangle ROM instances.
- The active testbench BRAM model was expanded from four to ten words and now drives a startup commit and one tracking commit.

### Static Review Result: Not Accepted

1. `shadow_ctrl_run` is used directly to decide whether the DDS runs. Writing the shadow control word changes behavior before `COMMIT_SEQ`, so the design violates its atomic-update contract. A running-register enable must be changed only on the apply edge.
2. A transaction with `RUN=0` cannot update `last_commit_seq` because commit handling is inside `if (shadow_ctrl_run)`. The stop request is therefore not a valid acknowledged commit, and a later start can consume stale data.
3. While stopped, the design resets both phase accumulators but still registers ROM-derived DAC values. A triangle ROM at address zero is not necessarily midscale; the specified stopped-output behavior is not implemented.
4. Shadow registers have no reset values. Until every BRAM word has completed at least one poll, simulation and hardware behavior depends on unknown/uninitialized state.
5. The BRAM polling address/data delay is consistent with the testbench's one-clock synchronous-read model, and the four ROM instances correctly avoid two-channel single-port ROM contention. These are the parts that passed static inspection.
6. The testbench waits fifteen DAC cycles after a phase-reload commit and then expects `phase_acc_a/b` to equal their initial values. A correct DDS advances on every cycle after the apply edge, so this assertion is invalid and cannot pass reliably.
7. The testbench does not check B-side running registers, stopped midscale output, exact commit-edge simultaneity, registered A/B waveform independence, 0 to 180 degree sine/sine phase cases, or phase-continuous tracking for both channels. Its tracking assertion only rejects a reset to zero and can miss other discontinuities.

### Verification Status

- Static RTL/TB review completed against `H_top.v`, the existing BRAM interface, and the synchronous BRAM model.
- Initial shell-path lookup did not locate a simulator. Subsequent path discovery found Vivado 2020.2 XSIM and the regression was run after the corrective changes below.

## 2026-07-18 - Dual-DDS Review Fixes Applied

### Authorized Source Changes

- `ad9767.sv` now resets all shadow control registers and adds `run_enabled` as the sole running enable.
- A changed shadow RUN bit has no DDS effect until `COMMIT_SEQ` changes. The commit edge now acknowledges both RUN and STOP transactions, updates all running fields, and either reloads both phase accumulators together, advances them with the new tracking step, or clears them for STOP.
- DAC A/B data are forced to midscale when stopped and on the commit edge, preventing an uninitialized ROM value or triangle address zero from reaching the DAC while output is disabled.
- `tb_H_top.v` now checks reset determinism, no uncommitted field leakage, both A/B running registers, independent A/B phase advance and data, uncommitted RUN isolation, exact tracking-commit continuity, all 37 settings from 0 to 180 degrees in 5 degree steps for sine/sine reload, and `RUN=0` DAC midscale behavior.
- The first XSIM run exposed an additional transaction bug: a newly written `COMMIT_SEQ` could be polled before the preceding new field had reached its shadow register. `ad9767.sv` now records a pending sequence, completes one full `0x00..0x20` shadow scan, and applies the snapshot only at the following `0x24` polling slot.

### Verification Status

- Static dataflow review completed: the BRAM address poll remains one address per DAC clock, and the TB's registered BRAM read still supplies the data associated with `cfg_sel_d` one clock later.
- Vivado 2020.2 XSIM compilation, elaboration, and `-runall` execution passed. The active TB reached all five PASS stages and called `$finish` at 7385 ns.
- XSIM warnings are limited to expected behavioral-model limitations for the independent-clock FIFO and block-memory collision behavior. They do not report an RTL assertion failure.
- Next verification: run the same TB in the configured Questa flow, inspect the apply edge and DAC outputs in the waveform viewer, then verify the 14-bit DAC timing and amplitude on hardware.

## 2026-07-18 - Wide-Window Waveform Review

### Artifact Reviewed

- User-provided ModelSim waveform screenshot: `C:/Users/32001/AppData/Local/Temp/codex-clipboard-64f2fa72-21c1-4111-9206-6bf0b7440710.png`.

### Result

- The transition to `da_data_a = da_data_b = 8192` at approximately 6.9 to 7.0 us is expected. It is the final TB stage, which commits `RUN=0`, clears both phase accumulators, and verifies that both DAC outputs remain at midscale before `$finish` at 7385 ns.
- The apparent sawtooth or discontinuous output before that point is not a steady-state waveform-quality failure in this TB view. The run contains startup, an A-step tracking commit, and then 37 phase-reload commits for 0 through 180 degrees in 5 degree increments. Each reload intentionally changes phase, so a full-width trace necessarily contains discontinuities.
- The screenshot is therefore valid evidence for the stop-to-midscale state and repeated control transactions. It is not sufficient evidence for sine/triangle distortion, DAC settling, or continuous tracking quality.

### Required Follow-Up Observation

- Zoom into stage two after startup and before the tracking commit to inspect normal A/B DDS continuity.
- Zoom into stage three around the tracking apply edge to confirm no phase reload occurs when only the phase step changes.
- Inspect the final STOP interval separately to confirm both outputs remain exactly 8192.
# 2026-07-20 - EMIO Buttons and Live DDS B-Phase Adjustment

## Changed

- Enabled a three-bit active-low PS EMIO GPIO input and connected
  `pl_key_i[2:0]` to PS7 `GPIO_I`: N16 is reset, T17 is phase increment, and
  R17 is phase decrement. The XDC applies `LVCMOS33` and internal pull-ups.
- Regenerated the BD wrapper and exported `system.tcl`. The script now avoids
  invalid project-save calls, and the project enables the generated wrapper
  instead of its stale imported copy.
- Defined `DDS_CTRL[2]` as B phase-delta mode. At its atomic commit edge, A
  advances normally and B advances normally plus the signed word at `0x18`.
  This permits runtime +/-5-degree B adjustments without reloading A.
- Extended `tb_H_top.v` with a live B-phase regression stage.

## Verification

- `validate_bd_design`, XSIM behavioral regression, and `synth_1` completed
  successfully. The new XSIM stage verified A phase continuity and a B
  five-degree increment on the same commit transaction.
