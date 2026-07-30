# IQ 解调下放 PL 与 DDS Compiler 设计决策

> 历史设计说明：IQ 解调硬件仍保留在工程中，但当前 2026 G 主链使用
> `adc_fir_axis -> SG DMA -> g26_measurement_task`，不配置或读取 IQ/DDC，
> 也不使用 DDS/DAC 输出。当前事实见
> `../../Doc/26G_当前架构与实测状态.md`。除非题目方向再次改变，不按本文
> 方案继续扩展当前应用。

## 结论

本工程第一期应封装并下放 **IQ 解调器**，不应先封装 IQ 调制器。

理由不是 IQ 调制没有价值，而是当前硬件的数据面决定了优先级：AD9226 已经在
约 5.12006 MHz 的采样时钟域持续产生 12 bit 样本；在这个流上增加 DDC
（数字下变频）只需本振、两个乘法器和窗口累加器，就能把一个窗口的幅度/相位压缩为
少量寄存器。反之，IQ 调制需要有 I/Q 基带样本源、符号定时/插值、成形滤波与 DAC
125 MHz 域的数据接口。当前 DAC 侧只有 PS 写 BRAM 后驱动的两个单音 DDS，没有高速
基带 AXIS/BRAM 数据面。为 IQ 调制先改它，会扩大 BD、DDS_DAC IP、PS 协议及 DAC
时序的修改范围，却不能直接改善现有 ADC 测量链。

适合本工程的第一期目标是“单频锁相检测器”，而不是试图替代所有 FFT：

```text
AD9226 capture clock domain (about 5.12006 MHz)
ADC signed sample x sin(NCO) --> I accumulator --+
ADC signed sample x cos(NCO) --> Q accumulator --+--> result registers --> PS
                                                  window_done/sequence
```

它适用于已知激励频点的幅频/相频、阻抗、锁相测量和扫频。PS 保留原始 DMA/FFT 路径，
用于未知频率搜索、双信号分离、波形判别和调试。这样一个题目不适合 IQ 时，系统仍有
完整后备路径。

## 当前工程约束

- `H_top.v` 中 AD FIFO 的写侧由 `w_ad_phase` 驱动；该时钟来自 `PLL_AD`，当前
  变更记录写为约 5.12006 MHz。PS 的旧常量是 5.12080 MHz，两者在任何精确频率换算
  前必须统一，不能假定相等。
- DAC DDS 使用 `i_clk_dac = 125 MHz`。ADC 和 DAC 并非同一时钟域，不能将 DAC 域
  的 DDS 正弦/余弦样本直接接到 ADC 域乘法器。
- 当前 `iq_demodulator.sv` 只有输入居中代码，尚未接入 `H_top`、AXI-Lite 或 DMA；
  本文代码是推荐的参考实现，不代表现有 bitstream 已具有 IQ 功能。
- ADC 的 12 bit 无符号码值应先去中点转为 signed。AD9226 板级传输函数在现有骨架中
  标为 `2048 - Vin * 1024`，故代码以 `2048 - raw` 定义正方向；如果板测极性相反，
  只改变这一处符号，I/Q 的幅度不变、相位整体翻转 180 度。

## DDS Compiler 方案是否可行

可行，并且比手写大 ROM/NCO 更适合做这一期通用核。应新建 **独立的** Xilinx
DDS Compiler IP，例如 `dds_iq_lo`；不要复用或改动当前 `DDS_DAC` 自定义 IP。
它的 `aclk` 必须接 ADC 样本处理时钟 `w_ad_phase`，每一拍产生与一个 ADC 样本严格
对应的 sin/cos。本振频率控制字与 DDS 输出都留在该时钟域。

设实际 ADC 域时钟为 `Fs_adc`，DDS 的相位累加器宽度为 `P`，期望解调频率为 `f_lo`：

```text
PINC = round(f_lo / Fs_adc * 2^P)
f_lo_actual = PINC * Fs_adc / 2^P
```

若要使 DAC 激励与 ADC 解调的频率相同，应由 PS 从同一个目标频率分别计算：

```text
PINC_dac = round(f_target / 125000000 * 2^32)
PINC_adc = round(f_target / Fs_adc * 2^32)
```

不能把 `PINC_dac` 原样用于 ADC DDS Compiler。频率一致不等于绝对相位一致：现有 ADC
和 DAC 时钟没有声明共同相位基准，I/Q 的绝对相位会包含两时钟的初始相位、CDC 装载时刻、
ADC/DAC 模拟延迟和被测链路延迟。幅度测量不受这一点影响；相频测量应固定每次的配置
加载/触发流程，并先以已知直通参考校准常数相位。若题目要求跨多次启动的绝对相位，须让
ADC/DAC 时钟来自同一参考源，并增加一个跨域同步的相位清零事件。

## DDS Compiler 配置

以下针对 Vivado 2020.2 的 DDS Compiler GUI。未列出的选项保持默认。实际生成后，
以 IP 生成的 `.veo` 端口模板和 `m_axis_data_tdata` 位域为准；不同小版本的 GUI
选项名可能略有不同。

| 页面/参数 | 填写值 | 原因 |
| --- | --- | --- |
| Component Name | `dds_iq_lo` | 与当前 `DDS_DAC` 自定义 IP 分离，职责是 ADC 域本振。 |
| Configuration / Functional Selection | `Phase Generator and SIN/COS LUT` | 同时需要 NCO 相位累加和正交 sin/cos 输出；只选 Phase Generator 不会产生乘法器本振。 |
| Channels | `1` | 一个 ADC 通道只需一对正交本振；多通道会复制 LUT/逻辑，没有当前收益。 |
| Phase Width | `32` | 与当前 PS 的 32 bit DDS 控制字一致。频率字分辨率为 `Fs_adc / 2^32`，在 5.12006 MHz 下约 0.00119 Hz，远小于 ADC 时钟误差和模拟误差。 |
| Output Width | `16` | 比 ADC 12 bit 多 4 bit，乘积精度足够且资源温和；16 bit signed sin/cos 的满量程约为 +/-32767。14 bit 也可运行，但没有必要与 DAC ROM 位宽强绑定。 |
| Output Data | `SIN and COS`，Signed Fraction | I/Q 两路乘法必须同时取得正交本振；选择 signed fraction 可直接做有符号乘法，避免 DAC 所用的 unsigned-midscale 编码。 |
| Phase Increment | `Programmable`，通过 `s_axis_phase_tdata[P-1:0]` 输入 | 扫频、锁相或题目换频时由 PS 写控制字，不必重新生成 IP；PINC 要以一次原子配置在 ADC 域装载。 |
| Phase Offset | `Programmable` | 用于相位归零、校准或已知参考的固定相移。第一期可一直写 0，但打开它的资源代价小，且避免日后重配 IP。 |
| Resync | `Enabled` | 公开 `s_axis_config`/resync 事件，在窗口边界让 NCO 相位从已知点开始。它使同一采集内的相位可重复；不能单独解决 ADC/DAC 两独立时钟的绝对相位问题。 |
| Noise Shaping | `None` | IQ 的积分窗口会平均量化噪声；抖动或 Taylor 校正会增加资源/延迟，并使第一期定点标定更复杂。32 bit phase 和 16 bit output 已足够。 |
| TREADY | `Disabled`（若 GUI 提供） | 本振链不允许因下游 back-pressure 停顿，否则每个 ADC 样本不再对应一个 NCO 样本。后级必须设计为每拍可接收，或以 `sample_valid` 同时门控。 |
| Output/Latency Configuration | 选择可见的 `Minimal` 或 `Auto`，并记录生成报告的固定 latency `L_dds` | DDS 是流水 IP，ADC 数据必须延迟同样的 `L_dds` 拍再相乘。不要假设延迟为 0；代码中的 `adc_delay` 参数必须按生成后报告/仿真填写。 |
| Clock Frequency / `aclk` | `Fs_adc`（当前暂按 5.12006 MHz，待 PLL 修正后改为 5.12080 MHz） | 频率字公式和时序约束依赖这个值。不得填 125 MHz，也不要使用当前 `DDS_DAC.xci` 中错误标为 100 MHz 的元数据。 |

### 配置接口的使用规则

1. PS 将新的 PINC、POFFSET、窗口长度写到 IQ 控制寄存器的 shadow bank。
2. PS 写 `COMMIT_SEQ`；它经双触发器/请求应答 CDC 到 ADC 域。
3. ADC 域仅在当前窗口结束后接收这组配置，向 DDS Compiler 发送一拍有效的 phase
   increment/offset/resync 配置，并清零 I/Q 累加器。
4. 在 DDS 核配置生效和其流水线填满前丢弃 `L_dds` 个对应样本；随后开始计窗口。
5. 结果寄存器采用 `result_seq` 或 valid toggle 锁存，PS 读取两次序号一致才使用 I/Q。

第一版若不做 AXI-Lite，可先把 `i_pinc`、`i_poff`、`i_window_len` 作为同一 ADC 时钟域
的测试输入。真正接入 PS 时必须补上上述 CDC 和 shadow/commit；不得把 100 MHz AXI
控制寄存器直接作为 5.12 MHz 逻辑的多位输入。

## 参考 SystemVerilog：DDS Compiler 驱动的单频 IQ 解调器

下列代码只用于说明 PL 逻辑与位宽。它不是对现有 RTL 的补丁，故未接入 `H_top`、
`ad_fifo_output`、BD 或 AXI-Lite。模块假定 DDS Compiler 被配置为 32 bit 可编程 PINC、
16 bit signed SIN/COS。`m_axis_data_tdata` 的 sin/cos 片段顺序必须在生成 IP 的 `.veo`
中确认；以下按常见配置假定低 16 bit 为 SIN、高 16 bit 为 COS。

```systemverilog
module iq_demodulator_dds #(
    parameter int ADC_WIDTH       = 12,
    parameter int LO_WIDTH        = 16,
    parameter int PHASE_WIDTH     = 32,
    parameter int ACC_WIDTH       = 48,
    parameter int DDS_LATENCY     = 4,   // Fill from generated DDS report.
    parameter int MAX_DDS_LATENCY = 16
) (
    input  logic                   clk_adc,
    input  logic                   rst_n,

    input  logic                   i_sample_valid,
    input  logic [ADC_WIDTH-1:0]   i_adc_raw,
    input  logic                   i_enable,
    input  logic                   i_cfg_apply, // one ADC-clock pulse after commit CDC
    input  logic [PHASE_WIDTH-1:0] i_pinc,
    input  logic [15:0]            i_window_len,

    output logic signed [ACC_WIDTH-1:0] o_i_sum,
    output logic signed [ACC_WIDTH-1:0] o_q_sum,
    output logic [15:0]                 o_sample_count,
    output logic                        o_ready,
    output logic [31:0]                 o_result_seq
);

    logic [PHASE_WIDTH-1:0] phase_tdata;
    logic                   phase_tvalid;
    logic [2*LO_WIDTH-1:0]  lo_tdata;
    logic                   lo_tvalid;

    // Check the generated .veo file. If it says the packing is different,
    // swap these two slices here, not elsewhere in the signal path.
    logic signed [LO_WIDTH-1:0] lo_sin;
    logic signed [LO_WIDTH-1:0] lo_cos;
    assign lo_sin = lo_tdata[LO_WIDTH-1:0];
    assign lo_cos = lo_tdata[2*LO_WIDTH-1:LO_WIDTH];

    // Delay the ADC sample so it aligns with the fixed DDS output latency.
    logic signed [ADC_WIDTH:0] adc_pipe [0:MAX_DDS_LATENCY-1];
    logic                       valid_pipe [0:MAX_DDS_LATENCY-1];
    logic signed [ADC_WIDTH:0] adc_centered;
    logic signed [ADC_WIDTH+LO_WIDTH:0] i_product;
    logic signed [ADC_WIDTH+LO_WIDTH:0] q_product;
    logic signed [ACC_WIDTH-1:0]       i_acc;
    logic signed [ACC_WIDTH-1:0]       q_acc;
    logic [15:0]                       sample_count;
    integer k;

    // `phase_tvalid` belongs to the DDS Compiler phase-control interface.
    // Adapt port names to the exact generated template. A real wrapper also
    // sends phase offset/resync through the generated config-word format.
    dds_iq_lo u_dds_iq_lo (
        .aclk                    (clk_adc),
        .s_axis_phase_tvalid     (phase_tvalid),
        .s_axis_phase_tdata      (phase_tdata),
        .m_axis_data_tvalid      (lo_tvalid),
        .m_axis_data_tdata       (lo_tdata)
    );

    always_ff @(posedge clk_adc or negedge rst_n) begin
        if (!rst_n) begin
            phase_tdata <= '0;
            phase_tvalid <= 1'b0;
            adc_centered <= '0;
            for (k = 0; k < MAX_DDS_LATENCY; k++) begin
                adc_pipe[k] <= '0;
                valid_pipe[k] <= 1'b0;
            end
        end else begin
            // Only a shadow/commit event may load a new PINC. Leaving this
            // valid high every sample could repeatedly reload the DDS state.
            phase_tvalid <= i_cfg_apply;
            if (i_cfg_apply) begin
                phase_tdata <= i_pinc;
            end

            // AD9226 code is unsigned. Center and choose the board's
            // documented polarity before signed multiplication.
            adc_centered <= $signed({1'b0, 12'd2048}) -
                            $signed({1'b0, i_adc_raw});
            adc_pipe[0] <= adc_centered;
            valid_pipe[0] <= i_sample_valid && i_enable;
            for (k = 1; k < MAX_DDS_LATENCY; k++) begin
                adc_pipe[k] <= adc_pipe[k-1];
                valid_pipe[k] <= valid_pipe[k-1];
            end
        end
    end

    assign i_product = adc_pipe[DDS_LATENCY-1] * lo_cos;
    assign q_product = adc_pipe[DDS_LATENCY-1] * lo_sin;

    always_ff @(posedge clk_adc or negedge rst_n) begin
        if (!rst_n) begin
            i_acc <= '0;
            q_acc <= '0;
            o_i_sum <= '0;
            o_q_sum <= '0;
            sample_count <= '0;
            o_sample_count <= '0;
            o_ready <= 1'b0;
            o_result_seq <= '0;
        end else begin
            o_ready <= 1'b0;
            if (!i_enable) begin
                i_acc <= '0;
                q_acc <= '0;
                sample_count <= '0;
            end else if (lo_tvalid && valid_pipe[DDS_LATENCY-1]) begin
                if (sample_count == i_window_len - 1'b1) begin
                    // Include the final product in the published result.
                    o_i_sum <= i_acc + i_product;
                    o_q_sum <= q_acc + q_product;
                    o_sample_count <= i_window_len;
                    o_ready <= 1'b1;
                    o_result_seq <= o_result_seq + 1'b1;
                    i_acc <= '0;
                    q_acc <= '0;
                    sample_count <= '0;
                end else begin
                    i_acc <= i_acc + i_product;
                    q_acc <= q_acc + q_product;
                    sample_count <= sample_count + 1'b1;
                end
            end
        end
    end
endmodule
```

### 代码进入工程前必须修正的接口细节

- DDS Compiler 中“phase increment”和“phase offset/resync”常通过不同的配置字段或
  AXIS config 通道给出。上例用 `s_axis_phase_*` 展示 PINC 的核心数据流；不能据此
  假设 POFFSET 已被实际写入。生成 IP 后按 `.veo` 的端口、配置 word 格式补齐。
- `i_cfg_apply` 必须是 ADC 域内的一拍 commit 脉冲。实际 DDS 配置接口只在 reset 后或
  一次 commit 时发送一次 PINC，随后 DDS 自由运行；不能让 phase valid 长期为高并每拍
  写 PINC，否则某些配置会反复重置相位。真正接入 AXI-Lite 时应实现
  `config_pending/config_accepted` 状态机。
- `DDS_LATENCY` 必须小于等于 `MAX_DDS_LATENCY` 且大于零。先跑 DDS 官方行为仿真，
  在 `s_axis_phase_tvalid` 到首个有效 `m_axis_data_tvalid` 之间数周期，再填写该值。
- `i_window_len` 必须非零；量产代码要在配置提交时拒绝 0，并定义最大窗口。4096 点时，
  ADC 12 bit 与 LO 16 bit 的乘积约 28 bit，累加增加 12 bit，40 bit 已可表示，48 bit
  留出裕量和后续滤波/校正空间。
- 发布 I/Q 时采用 shadow `o_i_sum/o_q_sum` 和 `o_result_seq`，避免 PS 读到累加中的撕裂
  数据。AXI-Lite 读跨域仍需稳定握手或异步 FIFO；单纯把这些寄存器连到 100 MHz 域不安全。
- 第一版不在 PL 中加入 CORDIC。PS 从已锁存的 I/Q 计算 `amplitude = scale*sqrt(I^2+Q^2)/N`
  和 `phase = atan2(Q,I)`，成本极小、标定透明。需要微秒级闭环更新时再添加 CORDIC IP。

## 为什么不是先做 IQ 调制

IQ 调制的正确链路为：`I/Q symbol -> pulse shaping/interpolation -> I*cos - Q*sin -> DAC`。
其中 I/Q 符号/样本必须按 DAC 域时钟稳定进入混频器；FM/FSK 又应改 NCO 相位增量而不是在
波形末端相乘。当前 `DDS_DAC` 的 BRAM 接口仅配置单音的波形、步长、相位和幅度，不是
持续基带样本接口。因此 IQ 调制的最小正确版本至少还需要：

1. AXIS 或双缓冲 BRAM 的 I/Q 数据源及 100 MHz 到 125 MHz CDC；
2. 码元定时、插值/成形滤波及欠载策略；
3. 125 MHz 域的两路乘法器、减法器、舍入/饱和与 DAC unsigned-midscale 编码；
4. 调制参数和样本流的原子切换；
5. 对频谱镜像、载波泄漏、DAC 模拟带宽和输出滤波的板级验证。

这些能力在以后做通信题时值得增加，但现在先做会破坏已验证的单音 DAC 路径，并且不能
降低 ADC 测量侧的 DMA/PS 压力。建议将 IQ 调制列为第二阶段独立 `nco_modulator` IP，
保持旁路，并通过新的 AXIS 数据面接入，绝不直接临时改写现有 `DDS_DAC` 的十字 BRAM
提交协议。

## 验证顺序

1. 先修正/确认 `PLL_AD` 的实际 `Fs_adc`，并让 PS、DDS 配置计算和仿真激励使用同一值。
2. 单独仿真 DDS Compiler：检查 sin/cos 打包、符号、幅度、固定延迟和 PINC 实际频率。
3. 写 `iq_demodulator_dds` 仿真：输入与 LO 同频的整数周期正弦，检查 Q 接近零、I 符号
   符合约定；对 +90 度输入检查 I/Q 互换；对不同频输入检查窗口积分衰减。
4. 加入 ADC 域后，先用内部已知测试样本，不接 DMA；检查窗口结束、`result_seq`、溢出和
   连续两窗口一致性。
5. 再接 AXI-Lite/CDC，验证配置只在窗口边界生效、PS 不会读到撕裂结果。
6. 板上以 DAC A 回接 ADC 为基准，标定 ADC/DAC 反相与固定相移，最后才测外部被测网络。
