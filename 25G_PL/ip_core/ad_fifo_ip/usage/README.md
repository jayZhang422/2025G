# ADC FIFO 输出 IP 使用说明

## 1. 功能与边界

`ad_fifo_warpper` 将 AD9226 的 12 位并行样本在相移采样时钟下寄存，经独立时钟 FIFO 送到 100 MHz 读侧。它同时导出与 FIFO 写入完全相同的 `adc_raw` 和 `sample_valid`，供 IQ 解调器观察；该观察接口不创建第二条 ADC 采样链。

本 IP 只负责 ADC 寄存、启动稳定等待和 CDC FIFO，不产生 AXI Stream、TLAST、DMA 或 IQ 结果。这些功能由上层 `H_top`、Block Design 和 IQ IP 实现。

## 2. 顶层端口

| 端口 | 时钟域/方向 | 说明 |
| --- | --- | --- |
| `rst_n` | 输入，低有效 | ADC 与 FIFO 复位。 |
| `clk_phase` | 输入，ADC 域 | 相移后的 AD9248 采样及 FIFO 写时钟，当前为 65 MHz。 |
| `adc_din[11:0]` | 输入 | AD9226 物理数据总线。 |
| `rd_clk` | 输入，读域 | 下游读取 FIFO 的时钟，当前 FCLK0 100 MHz。 |
| `rd_en` | 输入，读域 | 下游消费一个 FIFO 字的请求。 |
| `dout[15:0]` | 输出，读域 | `{adc_din[11:0], 4'b0}` 编码后的 FIFO 数据。 |
| `empty` | 输出，读域 | FIFO 可读数据不足时为高。 |
| `adc_raw[11:0]` | 输出，ADC 域 | 已寄存、稳定的原始 ADC 样本。 |
| `sample_valid` | 输出，ADC 域 | `adc_raw` 有效标志。 |

## 3. 时序与复位

`ad9226` 在释放复位后等待 64 个 `clk_phase` 周期，再将输入寄存到 `dout` 并拉高 `ad_out_valid`。`fifo` 在写时钟域额外保持 FIFO 复位 15 个周期，且在 FIFO reset busy、prog_full 或 prog_empty 时抑制非法读写。

`sample_valid` 只表示 ADC 域的样本有效，不能直接跨到 FCLK0。IQ IP 必须与 `adc_raw` 一起在 `clk_phase` 域使用；PS 只能通过其 AXI-Lite 结果接口读取 IQ 数据。

## 4. 集成方式

```text
AD9226 -> adc_din
clk_phase -> AD9226 采样和本 IP 写端
rd_clk(FCLK0) + rd_en(H_top AXIS 握手) -> dout/empty
adc_raw + sample_valid + clk_phase -> iq_demodulator_0
```

上层应依据 `empty` 和 AXIS `TREADY` 产生 `rd_en`，并在读域对 `dout` 形成 16 位 AXIS 数据。不要将 IQ 输入直接接到 `adc_din`，否则会绕过寄存和有效期控制。

## 5. 验证与维护

修改 ADC 时钟、FIFO 参数或采样相位后，应重新生成 `fifo_generator_0` 输出产物，检查 CDC/复位，并验证 ADC 稳定等待、FIFO 满空抑制、DMA 帧数据及 IQ 输入对齐。不要手工编辑 `.xci` 或 Vivado 生成缓存。
