# IP Inventory And Integration Reference

## Scope

This file records the active IP used by `25G_PL.xpr`. It is an architectural
reference, not a generated-IP cache. Configure vendor IP with Vivado, then
regenerate its products; do not hand-edit generated XCI, wrapper, cache, or
run-output files.

## Repository And Top-Level Rules

- The project IP repository is `25G_PL/ip_core`.
- `top.v` is the synthesis top. `H_top.v` contains the ADC/FIFO, AXIS, IQ
  observation, BRAM, and DAC integration wiring.
- The active BD wrapper is generated at
  `25G_PL.gen/sources_1/bd/system/hdl/system_wrapper.v`. The legacy copy in
  `25G_PL.srcs/sources_1/imports` is intentionally not in `sources_1`.
- Do not connect the IQ core directly to `i_ad_data`. Use the registered
  `adc_raw` and `sample_valid` outputs of `ad_fifo_wrapper_0` with `w_ad_phase`.

## Custom IP

| IP | Source | Clock/domain | Purpose and important interfaces |
| --- | --- | --- | --- |
| `ad_fifo_wrapper_0` | `ip_core/ad_fifo_ip` | ADC write domain plus FCLK0 read domain | Wraps `ad9226` and the independent-clock FIFO. Inputs: `rst_n`, `clk_phase`, `adc_din`, `rd_clk`, `rd_en`. Outputs include FIFO data/status, IQ observation, and passive monitor taps. The first valid FIFO write is aligned to a registered ADC sample. |
| `fifo_monitor_axi_0` | `ip_core/ad_fifo_monitor_axis/ad_fifo_monitor_axi_1.0` | ADC domain plus FCLK0 AXI4-Lite | Passively counts ADC samples, accepted FIFO writes, blocked writes, AXIS beats/frames/stalls, and publishes snapshots to PS at `0x43C1_0000`. |
| `DDS_DAC` | `ip_core/DDS_DAC_ip` | `clk_dac`, 125 MHz | Reads the ten-word BRAM control snapshot, applies changes at `COMMIT_SEQ`, and drives independent A/B DDS paths. It uses external sine/triangle ROM data and forces midscale while stopped. |
| `iq_demodulator` | `ip_core/iq_demodulator_ip_core` | ADC phase clock for DSP/DDS; FCLK0 for AXI-Lite | Single-frequency I/Q lock-in detector. ADC inputs are `clk_adc`, `i_adc_raw`, `i_sample_valid`, and `rst_n`. AXI-Lite input is `s_axi`; result IRQ is `o_irq`. The control/result CDC is inside this IP. |

## Vendor IP Outside The Block Design

| IP | Vendor core | Key configuration/use |
| --- | --- | --- |
| `PLL_AD` | Clocking Wizard 6.0 | Takes the 50 MHz board clock and produces the ADC 0-degree clock plus the phase-shifted ADC capture clock. The current DDS IQ clock-rate configuration is 5.12006 MHz. |
| `fifo_generator_0` | FIFO Generator 13.2 | Native 16-bit independent-clock FIFO inside `ad_fifo_wrapper_0`; ADC phase clock writes and FCLK0 reads. |
| `dds_iq_lo` | DDS Compiler 6.0 | 32-bit programmable phase increment and phase offset, 16-bit signed sine/cosine output, fixed latency 8, no TREADY. Its `aclk` metadata is 5,120,060 Hz and its LO frequency is set at runtime by IQ AXI-Lite configuration, not by this metadata. |
| `blk_rom_sine` | Block Memory Generator 8.4 | 4096 x 14 single-port sine ROM. Two instances serve DAC A and B. |
| `blk_rom_triangle` | Block Memory Generator 8.4 | 4096 x 14 single-port triangle ROM. Two instances serve DAC A and B. |
| `ila_0` | Integrated Logic Analyzer 6.2 | Retained project asset; not instantiated in the active top-level hierarchy. |

## Block Design IP

| Instance | Vendor core | Function |
| --- | --- | --- |
| `processing_system7_0` | Processing System 7 5.5 | Zynq PS7, DDR, FCLK0, GP0/HP0 AXI, UART, EMIO GPIO, and IRQ_F2P. |
| `proc_sys_reset_0` | Processor System Reset 5.0 | FCLK0 reset distribution for AXI peripherals. |
| `Pll_DA` | Clocking Wizard 6.0 | Produces 125 MHz `clk_dac` from FCLK0. |
| `axi_dma_adc` | AXI DMA 7.1 | Simple S2MM-only ADC DMA: 16-bit AXIS input and 64-bit memory interface. |
| `axis_data_fifo_0` | AXIS Data FIFO 2.0 | Buffers `ADC_STREAM_IN` before DMA S2MM. |
| `smartconnect_0` | SmartConnect 1.0 | Routes the DMA memory master to PS HP0 and PS GP0 slave path. |
| `axi_interconnect_0` | AXI Interconnect 2.1 | Routes PS GP0 AXI-Lite control to DMA, DDS BRAM controller, IQ, and FIFO monitor. |
| `axi_bram_ctrl_0` | AXI BRAM Controller 4.1 | PS AXI-Lite access to the DDS control BRAM. |
| `blk_PS_TO_PL` | Block Memory Generator 8.4 | True dual-port 2048 x 32 BRAM: PS uses port A; the DAC IP uses port B. |
| `iq_demodulator_0` | Local custom `iq_demodulator` 1.0 | IQ control/status at `0x43C0_0000`; ADC-domain sample inputs are exported through the generated wrapper; `o_irq` drives PS `IRQ_F2P`. |
| `fifo_monitor_axi_0` | Local custom `ad_fifo_monitor_axi` 1.0 | Read-only snapshot/status registers at `0x43C1_0000`; CONTROL provides snapshot and sticky-clear pulses. |
| `xbar`, `auto_pc` | AXI Crossbar / Protocol Converter | Generated internal support cells for the AXI interconnect. |

## IQ Control Contract

The IQ AXI-Lite address block starts at `0x43C0_0000`.

| Offset | Register | Meaning |
| --- | --- | --- |
| `0x00` | CTRL | Bit 0 is enable shadow; bit 1 toggles the configuration commit request. |
| `0x04` | PINC | Runtime 32-bit DDS phase increment. |
| `0x08` | POFFSET | Runtime 32-bit DDS phase offset. |
| `0x0C` | WINDOW | Nonzero ADC-sample integration length. |
| `0x10` | STATUS | Result-pending W1C, ADC-domain configuration acknowledgement, and commit-busy status. |
| `0x14` | RESULT_SEQ | Sequence number for a completed result snapshot. |
| `0x18` to `0x24` | I/Q result words | Signed 48-bit I and Q accumulator snapshots, low then high words. |
| `0x28` | SAMPLE_COUNT | Sample count belonging to the completed result. |

Software writes `PINC`, `POFFSET`, and `WINDOW` before committing CTRL. It
reads a completed result only after the IP publishes a new `RESULT_SEQ` or
signals `o_irq`; it must not assume an ADC-domain signal is directly readable
from FCLK0.

## Clock Change Maintenance Rule

The DDS output frequency remains programmable through `PINC`; changing ADC
sample frequency is a separate hardware change. When the PLL ADC rate changes:

1. Reconfigure `PLL_AD` in Vivado and record its actual output frequency.
2. Set `dds_iq_lo` `DDS_Clock_Rate` and `ACLK_INTF.FREQ_HZ` to that frequency.
3. Regenerate DDS and BD output products, then regenerate the wrapper.
4. Update the PS frequency conversion used to calculate IQ PINC.
5. Re-run BD validation and HDL syntax checks before simulation or synthesis.
