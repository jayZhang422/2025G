# PL Project Memory

## 2026-07-29 - 26G Functional Freeze

The authoritative current decision and measurement record is
[`../../Doc/26G_方案分析与工程复用评估.md`](../../Doc/26G_方案分析与工程复用评估.md).

For 26G, keep the implemented acquisition contract fixed unless a recorded
reopen condition is met:

```text
5.12006 MSPS ADC
-> ADC/FIFO CDC
-> 39-tap low-pass FIR, decimation 3
-> signed int16
-> 4096 samples / 8192 bytes / TLAST
-> AXIS FIFO
-> SG S2MM DMA
```

Do not change the ADC rate to 65 MSPS as a local adjustment. That would require
a new multistage decimator, ADC input timing closure, and coordinated PS/DMA/FFT
contracts.

PL may be reopened only for ADC input timing failure, a demonstrated
FIR/TLAST/backpressure defect, persistent overflow after correct continuous DMA,
or a proven analogue-alias failure that requires a sampling-rate redesign.

Known validation debt remains: converter-derived input delays, direct coverage
of `adc_fir_axis`, pre-FIR monitor semantics, and intermittent-capture overflow.
