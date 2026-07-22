# 2025G Vitis Architecture

## Status

The domain is `freertos10_xilinx`; active `identification_main.c` is still FreeRTOS Hello World. No 2025G workflow is implemented yet.

## Target modules

```text
app_state
|- board_buttons / display_ui
|- dma_capture
|- dds_control / wave_ram
|- calibration / known_model
|- rlc_learning
`- waveform_inference
```

Hardware access stays in drivers. Algorithms receive typed data and never raw PL addresses.

Data products:

- `calibration_t`: DAC amplitude and ADC gain/offset calibration.
- `complex_response_t`: learned frequency, gain and phase points.
- `rlc_model_t`: filter class and optional fitted coefficients.
- `wave_table_t`: 4096 unsigned 14-bit samples plus replay frequency.

Learning must finish in 120 seconds; basic and inference output must start within 5 seconds. Frequency estimation and replay use the common ADC/DAC reference ratio.


## Handoff status - 2026-07-22

Use repository-root CODEX_START.md as the only new-session entry. Both packaged PL IPs passed isolated behavioral regression, but the active XSA/platform contract has not changed. Do not implement PS access to arbitrary-wave RAM until PL integration, synthesis, XSA export, and BSP regeneration expose verified XPAR_* identifiers.
