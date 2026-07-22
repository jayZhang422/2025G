# 2025G Vivado Architecture

This is the authoritative PL context for new sessions. Target items are not implemented until a verification entry says so.

## Project

- Repository branch: `calvin`; Vivado project: `25G_PL.xpr`.
- Zynq-7020 `xc7z020clg400-2`, Vivado 2020.2.
- Synthesis top `top`; simulation top `tb_H_top`.
- Preserve existing interface names and parameter defaults unless explicitly approved.

## Requirement facts

The known model is `H(s)=5/(1e-8*s^2+3e-4*s+1)`. Its poles are about 607.9 Hz and 4166.7 Hz. At 3 kHz `|H| ~= 0.806`, so a 2 Vpp requested output needs about 2.48 Vpp input, not 12.4 Vpp.

The device must provide calibrated sine output through at least 1 MHz, learn an unknown one-R/one-L/one-C network in under 2 minutes, classify four filter types, and reproduce the response to 1-50 kHz periodic inputs within 5 seconds without visible frequency drift.

## Verified baseline

The active hierarchy still uses legacy RTL; the two packaged IPs exist but are not instantiated.

```text
AD9226 -> 5.1208 MHz capture -> async FIFO
-> 100 MHz AXIS (4096 accepted beats/TLAST)
-> AXIS FIFO -> simple S2MM DMA -> PS DDR

PS GP0 -> AXI BRAM controller -> ten-word control BRAM
-> atomic COMMIT_SEQ apply -> dual 125 MHz DDS -> AD9767 A/B
```

The DMA is S2MM-only and polled. Control BRAM and DMA identifiers must come from BSP `XPAR_*` symbols.

## Stable control-BRAM contract

| Offset | Meaning |
| --- | --- |
| 0x00/04/08/0C | A waveform/step/phase/amplitude |
| 0x10/14/18/1C | B waveform/step/phase/amplitude |
| 0x20 | RUN, PHASE_RELOAD and approved extension bits |
| 0x24 | COMMIT_SEQ, written last |

A changed sequence is applied only after a complete PL shadow scan. STOP is a valid commit and forces DAC midscale.

## Packaged IPs

- `ad_fifo_warpper` packages ADC registration and the async FIFO. Keep the existing spelling. It needs an external phase clock and lacks physical ADC clock generation and PLL-lock gating, so it is not a drop-in replacement.
- `DAC_DDS_Output` keeps defaults `32/12/14`, the atomic protocol and dual DDS. Waveform value 2 selects external arbitrary memory. Sine, triangle and arbitrary memories are outside the core; their ports and one-cycle latency must be integrated before replacement.

## Approved target

```text
common board reference
|- ADC clocking -> ADC/FIFO -> AXIS/DMA -> PS
`- DAC clocking -> DDS/arbitrary replay -> AD9767

control BRAM ----------------------> atomic DDS configuration
dedicated 4096x14 wave BRAM ------> arbitrary waveform reader
```

ADC and DAC clocks must derive from one board reference so measured frequency replays without oscillator-ratio drift. Use a dedicated PS-write/DDS-read waveform RAM; do not store the table in the 8 KiB control BRAM. Start with one bank because DDS is stopped while the table is built.

Analog requirements are board responsibilities: >=100 kOhm buffered input, ADC bias/protection/anti-aliasing, controlled input/output measurement selection, DAC reconstruction/gain and measured code-to-Vpp calibration.

## Verification gates

1. Fresh-clone script/path and BD validation.
2. Existing DDS/AXIS behavioral regression.
3. Packaged-IP compile and equivalence tests.
4. Arbitrary-wave RAM replay test.
5. Synthesis, implementation, timing and XSA export.
6. Vitis build against the exported XSA.
7. Board amplitude, impedance, coherent-frequency and end-to-end tests.

Static inspection alone never passes a gate.


## Verified progress - 2026-07-22

- script/pl_update_bd.tcl --check passed in Vivado 2020.2 from a fresh local calvin clone.
- Vivado loaded the repository-local user IP catalog at ip_core.
- script/pl_test_packaged_ips.tcl compiled DAC_DDS_Output, ad_fifo_warpper, ad9226, fifo, and FIFO Generator 13.2, then ran sim/ip/tb_packaged_ips.sv.
- XSim printed PACKAGED_IP_REGRESSION_PASSED and called $finish at 2506 ns.
- Regression covers DDS shadow isolation, arbitrary-mode atomic commit, phase reload, step advance, DAC falling-phase update, atomic stop/midscale, ADC stabilization, FIFO threshold release, changing samples, and 12-bit high alignment.
- This passes verification gate 3 only. Packaged IPs remain absent from the active top-level hierarchy; gates 4 through 7 remain open.
