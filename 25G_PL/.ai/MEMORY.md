# Project Memory

## New-session recovery

- The single reusable entry point is the repository-root FPGA_CodexPrompt.md.
- Paths in prompts and automation must be repository-relative or derived from the script location.
- A new session must separate implemented-and-verified, implemented-unverified, verified-not-integrated, and not-implemented states.
- The official problem PDF remains authoritative. If unavailable, do not guess disputed metrics or diagrams.

## Packaged-IP facts

- ad_fifo_warpper keeps its existing misspelled public name. Its empty output is FIFO Generator prog_empty with assert/negate thresholds 32/48, not the physical FIFO empty flag.
- DAC_DDS_Output keeps defaults PHASE_WIDTH=32, ADDR_WIDTH=12, DATA_WIDTH=14.
- DDS sine, triangle, and arbitrary ports expect external waveform memories; the regression models a one-clock synchronous read.
- A packaged-IP regression pass does not mean the IP is instantiated in the active top level.
