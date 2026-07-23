# PS algorithms

This directory contains typed, hardware-independent signal algorithms. Hardware
drivers remain in the sibling include/ and src/ directories.

| File pair | Responsibility |
|---|---|
| transfer_function_model.[ch] | Evaluate the known circuit transfer function H(jω). |
| dac_vpp_calibration.[ch] | Convert DAC amplitude code and measured Vpp through an offline curve. |
| open_loop_output_planner.[ch] | Plan basic items 3/4 without ADC/PID feedback. |
| coherent_transfer_measurement.[ch] | Calculate complex H(f) with synchronous I/Q demodulation. |
| rlc_filter_classifier.[ch] | Classify measured response as low-pass, high-pass, band-pass, or band-stop. |
| fundamental_frequency_estimator.[ch] | Search a configured frequency range for the input fundamental. |
| response_waveform_generator.[ch] | Generate a normalized unsigned 14-bit replay table from learned H_k. |
| two_channel_signal_analyzer.[ch] | Existing FFT-based two-channel sine/triangle analyzer. Its legacy function names are preserved for callers. |

The algorithms do not access XPAR addresses, AXI BRAM, DMA, GPIO, or UART.
They can be host-tested before being connected to the FreeRTOS application.