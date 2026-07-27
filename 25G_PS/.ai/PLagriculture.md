# PLagriculture - 2025G PS/PL Contract

Read this file before changing Vitis code.

## Current hardware

- Zynq-7020, Vivado/Vitis 2020.2.
- One 5.1208 MHz AD9226 capture channel.
- AXIS samples are 16-bit: ADC code in bits 15:4, zeros in 3:0.
- TLAST occurs on every 4096th accepted beat.
- One simple S2MM-only AXI DMA, polling, no connected interrupt.
- One AXI BRAM controller exposes the ten-word DDS control block.
- DDS/DAC clock is 125 MHz.
- Active RTL supports sine and triangle. Packaged arbitrary-wave IP is not integrated yet.

## Mandatory rules

- Use BSP `XPAR_*` identifiers for PL hardware.
- Invalidate the receive buffer after S2MM completion and before CPU access.
- Never edit generated BSP driver sources.
- Write all DDS shadow fields first and COMMIT_SEQ last.
- Build a waveform table while DDS is stopped, then atomically start it.
- Do not assume an IP is active merely because it exists under `25G_PL/ip_core`.

## Approved services

1. Basic: calculate known-model input amplitude and command calibrated sine DDS.
2. Learn: coherent sine sweep, complex response measurement, classification and model storage.
3. Infer: capture external periodic input, apply learned complex response, write a 4096-point arbitrary table, and start stable same-frequency replay within 5 seconds.

```text
BOOT -> MENU
        |- BASIC_SET -> BASIC_RUN
        |- LEARN -> MODEL_READY
        `- INFER_CAPTURE -> BUILD_WAVETABLE -> INFER_RUN
```

The measured complex response table is authoritative; an optional second-order fit supports interpolation and diagnostics.

## Current gap

The active Vitis entry is still FreeRTOS Hello World. Existing user modules mostly implement the 2023H separator and are reusable only as DMA, DDS, button and DSP examples.


## Verification state - 2026-07-22

The packaged ADC/FIFO and DDS/DAC IPs passed isolated Vivado/XSim compilation and behavioral checks at their existing names and default parameters. They are not instantiated in the active top level. PS software must continue using the verified legacy contract until a new synthesized XSA and its xparameters.h prove otherwise.


## Button and active-entry facts - 2026-07-25

- The active top exposes `pl_key_i[2:0]` to PS GPIO EMIO input width 3. `XGpioPs` logical pins 54, 55 and 56 are EMIO0, EMIO1 and EMIO2, not package-pin numbers.
- XDC mapping is `pl_key_i[0]=N16`, `pl_key_i[1]=T17`, and `pl_key_i[2]=R17`; MIO50 is package pin B13 and is the board PS_KEY1.
- `i_rst=N15` is a separate active-low PL hardware reset and is not readable as an application key through `XGpioPs`.
- The latest application entry contains Basic2 diagnostic and Basic3/Basic4 tasks; the earlier Hello World-only statement above is historical. Normal Basic3/Basic4 mode still requires `APP_DIAG_FORCE_DDS_TEST=0` at build time.
