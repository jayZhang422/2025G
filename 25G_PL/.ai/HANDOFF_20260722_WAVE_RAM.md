# G2025 Wave-RAM Integration Handoff (2026-07-22)

Read this record when a task touches the wave RAM, generated wrapper, or
related PL integration. It records unfinished integration work; it is not
evidence that the active hardware satisfies any G-question requirement.

## Verified facts

- The existing packaged-IP regression remains the gate-3 result recorded in
  `ARCHITECTURE.md`; the packaged IPs were not previously active in the top.
- `script/pl_test_g2025_dac.tcl` ran in Vivado 2020.2 and XSim printed
  `G2025_DAC_ADAPTER_V2_REGRESSION_PASSED`.
- That isolated regression compiled `DAC_DDS_Output.sv`,
  `g2025_dac_adapter_v2.sv`, and `tb_g2025_dac_adapter_v2.sv`. It checked
  arbitrary-wave selection, read-only waveform-memory access, DDS address
  advance, zero-amplitude DAC-B midscale, and atomic stop.
- The active `system.bd` now contains `axi_bram_ctrl_wave` and
  `blk_PS_TO_PL_WAVE`. Vivado `validate_bd_design` passed after connecting
  `axi_interconnect_0/M02_ACLK` and `M02_ARESETN`.
- `script/pl_update_bd.tcl` subsequently completed `generate_target all`,
  wrapper generation, and `system.tcl` export. The new PS address map is
  `axi_bram_ctrl_wave/S_AXI/Mem0 = 0x4200_0000`, range `0x4000`.
- Commit `bf0cb7b` saved the XPR source-list change: the active wrapper is
  now the generated `25G_PL.gen/sources_1/bd/system/hdl/system_wrapper.v`.
  The historical imported wrapper remains on disk but is no longer the
  active XPR entry. The selection flow is
  `script/pl_select_generated_wrapper.tcl`.

## Hardware contract of the new resource

- It is independent of the ten-word DDS control BRAM; `COMMIT_SEQ` remains
  the final control-BRAM write.
- Vivado 2020.2 AXI BRAM Controller v4.1 rejected the intended 16-bit
  asymmetric BRAM port. The validated BD uses a 4096-word, 32-bit true-dual
  port BRAM. Logical waveform samples are unsigned 14-bit values in bits
  `[13:0]` of each word; bits `[31:14]` are reserved and must be zero.
- PS writes this RAM only while DDS output is stopped. The intended DDS
  client reads it only; it must not write or reuse the control BRAM.

## Not integrated / not verified

- The XPR wrapper selection is saved as described above. This does not by
  itself establish a G2025 top-level synthesis or arbitrary-wave replay
  result.
- No G2025 top-level synthesis, implementation, timing, fresh XSA export,
  BSP regeneration, Vitis build, or board test has passed after this
  wave-RAM work.
- `g2025_top.sv`, `g2025_top_v3.sv`, `g2025_dac_adapter.sv`, and
  `g2025_dac_adapter_v3.sv` are exploratory, unintegrated files. Do not add
  them to `sources_1` or select them as synthesis top without a full source
  review and an isolated compile. In particular, `g2025_top_v3.sv` needs a
  wrapper-port binding review before use.
- No PS G2025 application, phase-sensitive-detection implementation, Levy
  fit, arbitrary-wave RAM HAL, or calibration code has been added.

## Algorithm decisions confirmed with the user

- Here PSD means phase-sensitive detection / orthogonal demodulation, not
  power spectral density. Learning should measure complex response with I/Q
  correlation against the known DDS excitation.
- The final periodic replay is not a real-time PS software filter: PS builds
  a waveform table before start, then PL/DDS replays it deterministically.
  CPU scheduling and DMA latency must not be in the per-sample DAC path.
- Levy rational fitting is an optional smoothing/interpolation aid; measured
  complex response remains authoritative.
- Basic requirements (3) and (4) must not use ADC/PID output feedback: the
  problem forbids feedback connections from the known-model output. Use
  `H(j*2*pi*f)` plus offline DAC calibration in the one-shot open-loop setup.

## Safe next actions

1. Read the core recovery section of `FPGA_CodexPrompt.md` and this handoff when
   the task concerns wave-RAM or PL integration.
2. Inspect `git status --short` before touching any generated or exploratory
   files; this worktree is intentionally dirty and has no commit.
3. Use the saved generated-wrapper selection in the current XPR. For any
   new-top compile, preserve the historical imported wrapper and do not
   overwrite the active project through `save_project_as`.
4. After a compile pass, add only one reviewed G2025 top and one reviewed
   adapter, then run synthesis before exporting an XSA.
5. Update the existing architecture/changelog only after the selected top
   has a named verification result. Do not claim arbitrary-wave PL
   integration complete before that point.