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

# 算法目录说明

该目录包含**具有明确数据类型、与硬件无关的信号处理算法**。所有硬件驱动程序位于同级的 **include/** 和 **src/** 目录中。

| 文件 | 职责 |
|---|---|
| transfer_function_model.[ch] | 计算已知电路的传递函数 H(jω)。 |
| dac_vpp_calibration.[ch] | 根据离线标定曲线，在 DAC 幅度码值与实际测得的峰峰值（Vpp）之间进行转换。 |
| open_loop_output_planner.[ch] | 在不依赖 ADC 或 PID 反馈的情况下，规划基础功能第 3/4 项的输出。 |
| coherent_transfer_measurement.[ch] | 使用同步 I/Q 解调计算复数形式的传递函数 H(f)。 |
| rlc_filter_classifier.[ch] | 根据测量得到的频率响应，将电路分类为低通、高通、带通或带阻滤波器。 |
| fundamental_frequency_estimator.[ch] | 在预先配置的频率范围内搜索输入信号的基波频率。 |
| response_waveform_generator.[ch] | 根据学习得到的 Hₖ，生成用于回放的归一化无符号 14 位波形查找表。 |
| two_channel_signal_analyzer.[ch] | 现有的基于 FFT 的双通道正弦/三角波分析器。 |

## 硬件独立性

这些算法**不会直接访问任何硬件资源**，包括：

- XPAR 地址（硬件外设基地址）
- AXI BRAM
- DMA
- GPIO
- UART

因此，它们可以先在主机环境（Host）下独立进行测试和验证，然后再集成到 FreeRTOS 应用程序中。