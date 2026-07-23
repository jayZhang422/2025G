# IQ 解调器 IP 使用说明

## 1. 功能与边界

`iq_demodulator` 是工作在 ADC 采样时钟域的可编程数字正交解调器。它使用
DDS Compiler 产生同相和正交本振，将去中码后的 ADC 样本分别与 cos/sin 相乘，
并在可配置窗口内累加为 I/Q 结果。

本 IP 提供两种工作方式：

- 单频连续模式：对一个固定 `PINC` 连续产生窗口化 I/Q 结果；每个结果必须由
  PS 确认后才开始下一个窗口。
- 顺序扫频模式：从起始 `PINC` 开始，以可正可负的步进依次测量多个频点；可设置
  单次扫描或循环扫描。

它不计算幅值、相位、峰值、频率选择或波形类型，不包含 CORDIC。PS 应读取稳定的
原始 I/Q 累加值后完成这些浮点或定点计算。顺序扫频可以测量基波及多个谐波，PS 可据此
实现正弦/三角波判定、双分量候选选择和其他题目相关规则。

该 IP 只有一套 DDS 和一条乘加流水线，扫频是**串行**执行，不是多个频点的并行 FFT。
它不影响现有 ADC -> FIFO -> AXIS -> DMA 通路；在等待 PS 确认结果时，仅暂停本 IP 的
下一次 I/Q 窗口。

## 2. 顶层连接

| 端口 | 时钟域/方向 | 连接要求 |
| --- | --- | --- |
| `clk_adc` | ADC 域输入 | 接 AD9226 的相移采样时钟。当前工程实际频率为 `5,120,060 Hz`。 |
| `rst_n` | ADC 域输入 | 低有效复位；与 ADC 数据路径的有效复位保持一致。 |
| `i_adc_raw[11:0]` | ADC 域输入 | 接已寄存且稳定的原始 ADC 码值，不能直接接异步 ADC 管脚。 |
| `i_sample_valid` | ADC 域输入 | 仅在对应 `i_adc_raw` 有效时置一。 |
| `s_axi_*` | FCLK/AXI-Lite 从接口 | 接 PS 的 `M_AXI_GP0` 经 AXI Interconnect 的控制通路。 |
| `o_irq` | FCLK 域输出 | 结果待读取时为高，接 PS `IRQ_F2P`；PS 清除结果待处理位后拉低。 |
| `o_i_sum/o_q_sum/...` | ADC 域输出 | 调试观察端口；软件必须通过 AXI-Lite 读取结果寄存器。 |

当前工程中 `H_top` 已将 `ad_fifo_output` 的 `adc_raw`、`sample_valid` 和相移 ADC
时钟输出到 `top`，再连接至生成的 `system_wrapper`。本 IP 重新封装后顶层接口未变化，
不需要改动 `H_top.v` 或 `top.v`。

## 3. 寄存器映射

基地址由 Vitis 导出的 `xparameters.h` 中
`XPAR_IQ_DEMODULATOR_0_BASEADDR` 给出。当前 BD 地址为 `0x43C0_0000`，软件不得在
应用代码中写死该数值。

| 偏移 | 名称 | 访问 | 说明 |
| ---: | --- | --- | --- |
| `0x00` | `CTRL` | RW | bit0 `ENABLE`；bit1 `COMMIT` 写 1 时提交当前 shadow 配置；bit2 `SCAN_ENABLE`；bit3 `SCAN_REPEAT`。读回 bit1 恒为 0。 |
| `0x04` | `PINC` | RW | 起始 DDS 32 位相位增量。 |
| `0x08` | `POFFSET` | RW | 每个频点使用的 DDS 32 位相位偏置。 |
| `0x0C` | `WINDOW` | RW | 每次积分的有效 ADC 样本数；0 不产生结果，最大值为 65535。 |
| `0x10` | `STATUS` | RO/W1C | bit0 `RESULT_PENDING`；向 bit0 写 1 确认当前结果。bit1 `CFG_ACK`；bit2 `CFG_BUSY`；bit3 `ADC_WAIT_ACK`。 |
| `0x14` | `RESULT_SEQ` | RO | 每完成一个结果递增一次。 |
| `0x18` | `I_SUM_LO` | RO | I 累加值低 32 位。 |
| `0x1C` | `I_SUM_HI` | RO | I 累加值高 16 位，位 31:16 为符号扩展。 |
| `0x20` | `Q_SUM_LO` | RO | Q 累加值低 32 位。 |
| `0x24` | `Q_SUM_HI` | RO | Q 累加值高 16 位，位 31:16 为符号扩展。 |
| `0x28` | `SAMPLE_COUNT` | RO | 本结果实际积分样本数，应等于已提交的 `WINDOW`。 |
| `0x2C` | `RESULT_PINC` | RO | 与当前 I/Q 快照严格对应的实际 `PINC`。 |
| `0x30` | `SCAN_STEP` | RW | 扫频相邻频点的 32 位补码 `PINC` 步进，可为负。 |
| `0x34` | `SCAN_COUNT` | RW | 扫频频点数；写 0 按 1 个频点处理。 |

`PINC` 与目标频率的关系为：

```text
PINC = round(f_target * 2^32 / F_adc)
F_adc = 5,120,060 Hz
```

扫频第 `k` 个频点（从 0 开始）为：

```text
PINC[k] = PINC + k * signed(SCAN_STEP)
```

软件应以 `RESULT_PINC` 而非自行推导的索引作为结果频率的最终依据。

## 4. 单频测量流程

1. 保持 `ENABLE=0`，写入 `PINC`、`POFFSET` 与非零 `WINDOW`。
2. 写 `CTRL = ENABLE | COMMIT`，即 `0x03`。每次写入 bit1 为 1 都会发起一次新的配置提交。
3. 轮询 `STATUS.CFG_BUSY=0` 且 `STATUS.CFG_ACK=1`，或等待配置完成后的第一笔结果中断。
4. 等待 `STATUS.RESULT_PENDING=1` 或 `o_irq` 上升。
5. 在待处理位仍为 1 时读取 `RESULT_SEQ`、I/Q、`SAMPLE_COUNT` 和 `RESULT_PINC`；这些寄存器在确认前保持不变。
6. 对 `STATUS` 写 `0x0000_0001`。IP 在完成跨时钟确认后，以同一频点重新配置 DDS 并开始下一窗口。

单频模式的最短相邻结果间隔约为：

```text
(WINDOW + DDS_LATENCY + 2) / F_adc + AXI 读取与确认时间
```

当前 DDS 固定延迟为 8 个 ADC 时钟，IP 在每次配置或频点切换后丢弃填充阶段的样本。

## 5. 顺序扫频流程

1. 写 `PINC` 为第一个频点，写 `SCAN_STEP` 与 `SCAN_COUNT`。
2. 写 `POFFSET`、`WINDOW`。建议窗口覆盖整数个参考周期；否则频率失配会导致 I/Q 泄漏。
3. 单次扫描写 `CTRL = ENABLE | COMMIT | SCAN_ENABLE`，即 `0x07`；循环扫描额外置 `SCAN_REPEAT`，即 `0x0F`。
4. 对每个 `RESULT_PENDING`：读取完整结果快照及 `RESULT_PINC`，保存后 W1C `STATUS[0]`。
5. 单次扫描在最后一个结果确认后停止。PS 根据 `SCAN_COUNT` 和已保存的结果数判断单次扫描已经结束；开始新扫描时再次写入 `CTRL` 的 `COMMIT` 位。

扫频切换发生在上一个结果被 PS 确认之后，不会在积分窗口中途更换 DDS 配置，也不会覆盖未读取的结果。`SCAN_REPEAT` 在最后一个点确认后回到初始 `PINC`。

## 6. PS 端结果处理

I/Q 为 48 位有符号累加值。PS 将高低字拼接为有符号 64 位值后，可采用：

```text
magnitude_raw = sqrt(I * I + Q * Q)
phase_rad     = atan2(Q, I)
```

若需要输入 ADC 码值幅度，还应按积分长度、DDS 正弦幅度、ADC 前端增益和实际标定系数
归一化。当前 RTL 使用：

```text
adc_centered = i_adc_raw - ADC_MIDSCALE
I += adc_centered * cos(LO)
Q += adc_centered * sin(LO)
```

实际板卡的 ADC 极性、模拟链路延迟与 DDS 相位零点会引入固定相位偏置，应使用已知参考
信号标定后再解释绝对相位。不要仅凭未标定的 `atan2` 结果判断硬件相位正确性。

## 7. 时钟域与软件约束

- AXI shadow 配置经请求翻转和同步后才在 ADC 域采样。`STATUS.CFG_BUSY=1` 时不得修改
  `PINC`、`POFFSET`、`WINDOW`、`SCAN_STEP` 或 `SCAN_COUNT`。
- 新结果通过 CDC 后锁存在 AXI 域；确认操作再通过独立翻转同步回 ADC 域。必须先读完整
  快照，后 W1C `RESULT_PENDING`。
- `o_irq` 是高电平保持中断，不是单周期脉冲。中断服务程序只应通知任务；任务读取结果并
  W1C 后，中断才会撤销。
- `rst_n` 和 `s_axi_aresetn` 都为低有效。系统启动时应先释放时钟和 ADC 数据路径复位，再
  进行一次显式配置提交。
- 本 IP 不替代原始 DMA 数据采集。面对未知宽带输入时，DMA+PS FFT 仍可作为频率搜索与
  调试后备；IQ 扫频更适合少量目标频点、扫频响应或谐波测量。

## 8. 静态验证范围

本说明对应的 RTL 已使用 Vivado 2020.2 `xvlog -sv` 完成语法分析。该检查不等同于行为仿真、
综合、时序收敛或板级幅相标定。重新封装或修改 DDS Compiler 参数后，至少应重新执行：

1. IP 重新封装并更新 BD 输出产物；
2. `validate_bd_design`；
3. 包含单频、负步进扫频、循环回卷、结果未确认保持和复位恢复的行为仿真；
4. 综合、时序检查和板级已知频率/相位校准。
