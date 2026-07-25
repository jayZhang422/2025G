# Change Log

## 2026-07-22 - Reusable Codex handoff and verified PL baseline

- Added repository-root FPGA_CodexPrompt.md as the only reusable new-session entry.
- Recorded mandatory read order, fact priority, Git safety checks, current recovery point, and verification boundaries.
- Corrected PS knowledge files to real Markdown line endings.
- Confirmed active Vitis entry remains FreeRTOS Hello World; no 2025G PS application or HAL is claimed complete.
- Recorded that both packaged PL IPs passed isolated Vivado/XSim regression but remain unintegrated.

## 2026-07-22 - 2025G architecture initialization

- Added the mandatory PL/PS contract and separated verified baseline from approved target.
- Recorded packaged-IP integration and active-main gaps.
- Corrected 3 kHz known-model gain to about 0.806 and required input to about 2.48 Vpp for 2 Vpp output.

## 2026-07-24 - XSCT platform recovery verified

- Diagnosed the legacy platform failure as an obsolete absolute XSA handoff path, E:/7020_Project/25G/25G_PL/top.xsa, rather than an application-source build error.
- Added script/ps_create_platform_from_xsa.tcl and Vitis_Workspace_Runbook.md for a GUI-independent recovery path.
- Created VitisWorkspaces/2025G_A/Identification_platform from the current 25G_PL/top.xsa after HSI preflight.
- Verified the resulting platform exposes ps7_cortexa9_0 and preserves the active DMA and control-BRAM XPAR address contract.

## 2026-07-24 - BSP static libraries generated

- Confirmed the first application link failure was caused by an empty exported bsplib/lib directory, not by a missing software project.
- Added ps_generate_platform_libs.tcl to run full platform generation and BSP archive creation for an existing isolated workspace.
- Verified the new platform export contains libxil.a and libfreertos.a.

## 2026-07-24 - Application target link completed

- Resolved the final ARM target link failure by adding the standard math library m for CMSIS and algorithm math functions.
- User reported the Debug application build completed successfully after the platform library and math-library fixes.

## 2026-07-24 - Basic3/Basic4 output path implemented

- Added the Basic3/Basic4 open-loop output planner with the required 100 Hz to 3 kHz frequency range, 100 Hz frequency step, 1.0 Vpp to 2.0 Vpp target range, and 0.1 Vpp target step.
- Added the known transfer-function coefficients and reused the existing model-magnitude, required-input, and DAC calibration planning path. The coefficients are b0=0, b1=0, b2=5, a0=1e-8, a1=3e-4, a2=1.
- Added a button-driven Basic3/Basic4 UI/task entry. In MENU, RESET selects the frequency/target field and the phase buttons adjust the selected field; START attempts a one-key DDS start.
- Added tests/test_basic_output.c; strict host self-test passes together with the existing state-machine and transfer-algorithm tests.
- ARM normal-mode object compilation and a manual target link pass. The generated Vitis source lists have not been regenerated, so the new sources still require a normal Vitis project refresh before claiming GUI/build-system integration.
- The runtime intentionally uses an empty DAC code-to-Vpp calibration curve. Until measured code/Vpp pairs are inserted, Basic3/Basic4 START reports DAC calibration required and does not emit a falsely calibrated output. The existing diagnostic DDS default remains unchanged.
## 2026-07-24 - First advanced learning gate

- Added rlc_learning_measure_scan to orchestrate coherent I/Q measurement over caller-owned captured frames, with no DMA, XPAR, or hardware-address assumptions.
- Added rlc_learning_summarize to classify the measured scan and report edge magnitudes and peak frequency/magnitude.
- Added tests/test_rlc_learning.c; strict host self-test and ARM object compilation pass.
- This is a verified software gate only. DMA scan scheduling, board capture, waveform-RAM write, and physical replay remain unintegrated and unverified.

## 2026-07-25 - DAC Vpp calibration integrated

- Recorded the final adjustable-gain DAC calibration measured at 1 kHz: 0/0.000 Vpp, 1024/0.346 Vpp, 2048/0.690 Vpp, 4096/1.390 Vpp, 6144/2.070 Vpp, 8191/2.750 Vpp, and 16383/5.470 Vpp.
- Replaced the Basic3/Basic4 empty calibration curve with the seven measured points; the temporary diagnostic amplitude macro was restored by the user.
- Updated the strict Basic3/Basic4 host test to use the measured curve and verify the 3 kHz, 2.0 Vpp boundary plan. The test passes.
- Full Vitis diagnostic-mode rebuild/link passes. A separate ARM compile/link with APP_DIAG_FORCE_DDS_TEST=0 also passes; physical Basic3/Basic4 output accuracy remains pending board measurement.


## 2026-07-25 - Fixed physical-button roles

- Preserved all legacy button interfaces and added semantic aliases/APIs: MIO50 START, EMIO54 STOP/BACK, EMIO55 LEARN, and EMIO56 SYSTEM RESET.
- Removed parameter selection and increment/decrement behavior from the active Basic3/Basic4 task. Initial test parameters remain 1 kHz and 2.0 Vpp and can be overridden at build time until the touch UART UI is integrated.
- Basic2 diagnostic mode now waits for START, supports STOP/BACK and RESET, and reports LEARN as unavailable instead of starting DDS automatically.
- Basic3/Basic4 LEARN enters the state-machine learning page stub and explicitly reports that runtime learning integration is pending; STOP/BACK returns to the menu.
- Host regressions, full diagnostic Vitis build/link, and normal-mode ARM build/link pass. Physical key identification and board behavior remain to be tested.
