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
