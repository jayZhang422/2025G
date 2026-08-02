# PL Project Memory

## 2026-08-02 - AD9248 Branch Override

The current G26 PL data path is:

```text
AD9248 one-channel 14-bit two's-complement at 5,120,060 Hz
 -> H_top raw 16-bit AXIS
 -> adc_fir_axis
 -> 39-tap Q1.17 low-pass FIR, decimation 3
 -> symmetric rounding, signed16 saturation
 -> 4096 samples / 8192 byte / TLAST
 -> AXIS FIFO -> SG S2MM DMA -> PS
```

FIR facts:

- passband `0..500 kHz`;
- 500 kHz `-0.008071 dB`;
- 1 MHz `-67.630308 dB`;
- stopband maximum `-67.149421 dB`;
- coefficient sum `2^17`;
- output sample rate `1,706,686.667 Hz`.

PS must decode the DMA frame as `signed int16`; raw ADC unpacking and a second
PS FIR are forbidden. The FIFO monitor observes the acquisition path before the
top-level FIR and is diagnostic only for post-FIR payload correctness.

The current G26 application does not use IQ/DDC or DDS/DAC even though those
blocks remain physically integrated. Do not change PL coefficients, scaling,
TLAST, DMA width, or sample rate in response to PS calibration data.

The 14-bit code is stored as `{adc[13:0], 2'b00}` in the existing 16-bit FIFO
word and restored to signed16 before the FIR. The retained `adc_raw[11:0]`
observation port stays 12-bit for BD compatibility. `AD9248.xdc` is the active
converter constraint file in this branch; the legacy `AD9226.xdc` is retained
but disabled. `top` holds `o_ad_oeb` (`W19`, J10 pin 3) and `o_ad_pdn` (`U17`,
J10 pin 19) low directly; these controls do not pass through `H_top`. OTR is
not exposed or constrained. ILA testing confirmed valid raw data on both
AD9248 A and B paths and the expected output coding. Input-delay constraints
are still required to prove capture setup/hold margin.

Board tests currently pass 1/1.5/2 MHz interference. Remaining PL-adjacent risk
is analog aliasing before ADC in the `4.62006..5.62006 MHz` window; once such a
signal folds into 0..500 kHz, the FIR cannot distinguish it from valid input.
