`timescale 1ns / 1ps

// =============================================================================
// PS/BRAM 控制数据结构说明 (偏移地址映射):
//   0x00: 通道 A 波形 (0正弦, 1三角)
//   0x04: 通道 A 频率步进
//   0x08: 通道 A 初始相位
//   0x0C: 通道 A 幅度 [13:0]
//   0x10: 通道 B 波形 (0正弦, 1三角)
//   0x14: 通道 B 频率步进
//   0x18: 通道 B 初始相位
//   0x1C: 通道 B 幅度 [13:0]
//   0x20: 系统控制 (Bit0: RUN使能, Bit1: PHASE_RELOAD重载使能)
//         Bit2: B 相位增量，0x18 作为有符号增量而非绝对初相
//   0x24: 原子提交序号 COMMIT_SEQ
// =============================================================================

module ad9767 (
    input  logic        clk,
    input  logic        rst_n,

    // BRAM 控制与数据读取接口
    output logic [31:0] bram_addr,
    input  logic [31:0] bram_dout,
    output logic        bram_en,
    output logic [3:0]  bram_we,

    // AD9767 双通道 DAC 物理接口
    output logic [13:0] da_data_a,
    output logic [13:0] da_data_b,
    output logic        da_wrt_a,
    output logic        da_wrt_b
);

    // =========================================================================
    // 内部信号声明
    // =========================================================================
    
    // 1. BRAM 轮询计数器
    logic [3:0]  cfg_sel;       // 当前请求地址
    logic [3:0]  cfg_sel_d;     // 延迟一拍的地址 (匹配数据读取延迟)

    // 2. 影子寄存器组 (只收数据，不影响当前输出)
    logic        shadow_wave_a, shadow_wave_b;
    logic [31:0] shadow_step_a, shadow_step_b;
    logic [31:0] shadow_phase_init_a, shadow_phase_init_b;
    logic [13:0] shadow_amp_a, shadow_amp_b;
    logic        shadow_ctrl_run;
    logic        shadow_ctrl_reload;
    logic        shadow_ctrl_b_phase_adjust;
    logic [31:0] shadow_commit_seq;

    // 3. 运行寄存器组 (真正送给 DDS 的控制字)
    logic        run_wave_a, run_wave_b; 
    logic [31:0] run_step_a, run_step_b;
    logic [13:0] run_amp_a,  run_amp_b;
    logic        run_enabled;

    // 4. 双通道相位累加器
    logic [31:0] phase_acc_a; 
    logic [31:0] phase_acc_b;

    // 5. 双通道 ROM 读取数据
    logic [13:0] sine_data_a,     triangle_data_a;
    logic [13:0] sine_data_b,     triangle_data_b;
    logic [13:0] waveform_data_a, waveform_data_b;

    // 6. 双通道计算中间变量
    logic signed [14:0] centered_a, centered_b;
    logic signed [29:0] scaled_a,   scaled_b;
    logic signed [15:0] dac_code_a, dac_code_b;
    logic [13:0] dac_sample_a, dac_sample_b;

    // 7. 原子提交触发器
    logic [31:0] last_commit_seq;
    logic [31:0] pending_commit_seq;
    logic        commit_pending;
    wire         commit_trigger = (shadow_commit_seq != last_commit_seq);
    wire         apply_commit = commit_pending && (cfg_sel_d == 4'd9);


    // =========================================================================
    // 基础组合逻辑赋值
    // =========================================================================
    
    assign bram_en   = 1'b1;
    assign bram_we   = 4'b0;
    
    // 地址映射：0~9 对应 0x00~0x24
    assign bram_addr = {26'd0, cfg_sel, 2'b0};

    assign da_wrt_a  = clk;
    assign da_wrt_b  = clk;


    // =========================================================================
    // ROM 模块例化 (双份，确保时序绝对安全)
    // =========================================================================

    // 通道 A 的 ROM
    blk_rom_sine sine_a (
        .clka  (clk), .ena(1'b1),
        .addra (phase_acc_a[31:20]), .douta (sine_data_a)
    );
    blk_rom_triangle triangle_a (
        .clka  (clk), .ena(1'b1),
        .addra (phase_acc_a[31:20]), .douta (triangle_data_a)
    );

    // 通道 B 的 ROM (复用同样的 IP，但分身实例化)
    blk_rom_sine sine_b (
        .clka  (clk), .ena(1'b1),
        .addra (phase_acc_b[31:20]), .douta (sine_data_b)
    );
    blk_rom_triangle triangle_b (
        .clka  (clk), .ena(1'b1),
        .addra (phase_acc_b[31:20]), .douta (triangle_data_b)
    );


    // =========================================================================
    // DDS 波形幅值独立计算 (纯组合逻辑)
    // =========================================================================
    always_comb begin
        // --- 通道 A 计算 ---
        waveform_data_a = run_wave_a ? triangle_data_a : sine_data_a;
        centered_a      = $signed({1'b0, waveform_data_a}) - 15'sd8192;
        scaled_a        = centered_a * $signed({1'b0, run_amp_a});
        dac_code_a      = (scaled_a >>> 14) + 16'sd8192;

        // --- 通道 B 计算 ---
        waveform_data_b = run_wave_b ? triangle_data_b : sine_data_b;
        centered_b      = $signed({1'b0, waveform_data_b}) - 15'sd8192;
        scaled_b        = centered_b * $signed({1'b0, run_amp_b});
        dac_code_b      = (scaled_b >>> 14) + 16'sd8192;
    end


    // =========================================================================
    // 核心时序控制：影子抓取、原子提交与相位累加
    // =========================================================================
    always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            cfg_sel           <= 4'd0;
            cfg_sel_d         <= 4'd0;
            last_commit_seq   <= 32'd0;
            pending_commit_seq <= 32'd0;
            commit_pending     <= 1'b0;

            shadow_wave_a       <= 1'b0;
            shadow_wave_b       <= 1'b0;
            shadow_step_a       <= 32'd0;
            shadow_step_b       <= 32'd0;
            shadow_phase_init_a <= 32'd0;
            shadow_phase_init_b <= 32'd0;
            shadow_amp_a        <= 14'd0;
            shadow_amp_b        <= 14'd0;
            shadow_ctrl_run     <= 1'b0;
            shadow_ctrl_reload  <= 1'b0;
            shadow_ctrl_b_phase_adjust <= 1'b0;
            shadow_commit_seq   <= 32'd0;
            
            phase_acc_a       <= 32'd0;
            phase_acc_b       <= 32'd0;
            
            run_wave_a <= 1'b0;  run_wave_b <= 1'b0; 
            run_step_a <= 32'd0; run_step_b <= 32'd0; 
            run_amp_a  <= 14'd0; run_amp_b  <= 14'd0; 
            run_enabled <= 1'b0;
            dac_sample_a <= 14'd8192;
            dac_sample_b <= 14'd8192;
            
        end else begin
            // ----------------------------------------------------
            // 1. BRAM 轮询与影子缓存更新
            // ----------------------------------------------------
            cfg_sel_d <= cfg_sel;
            cfg_sel   <= (cfg_sel == 4'd9) ? 4'd0 : cfg_sel + 1'b1;

            case (cfg_sel_d)
                4'd0: shadow_wave_a       <= bram_dout[0];
                4'd1: shadow_step_a       <= bram_dout;
                4'd2: shadow_phase_init_a <= bram_dout;
                4'd3: shadow_amp_a        <= bram_dout[13:0];
                4'd4: shadow_wave_b       <= bram_dout[0];
                4'd5: shadow_step_b       <= bram_dout;
                4'd6: shadow_phase_init_b <= bram_dout;
                4'd7: shadow_amp_b        <= bram_dout[13:0];
                4'd8: begin
                    shadow_ctrl_run            <= bram_dout[0];
                    shadow_ctrl_reload         <= bram_dout[1];
                    shadow_ctrl_b_phase_adjust <= bram_dout[2];
                end
                4'd9: shadow_commit_seq   <= bram_dout;
            endcase

            // ----------------------------------------------------
            // 2. 原子提交与相位累加逻辑
            // ----------------------------------------------------
            if (apply_commit) begin
                // A changed sequence is first held pending for one complete
                // 0x00..0x20 polling pass. Apply only after that snapshot pass.
                last_commit_seq <= pending_commit_seq;
                commit_pending  <= 1'b0;
                run_enabled     <= shadow_ctrl_run;
                run_wave_a      <= shadow_wave_a;
                run_wave_b      <= shadow_wave_b;
                run_step_a      <= shadow_step_a;
                run_step_b      <= shadow_step_b;
                run_amp_a       <= shadow_amp_a;
                run_amp_b       <= shadow_amp_b;

                if (!shadow_ctrl_run) begin
                    phase_acc_a <= 32'd0;
                    phase_acc_b <= 32'd0;
                end else if (shadow_ctrl_reload) begin
                    // A common reload edge establishes the requested A/B phase.
                    phase_acc_a <= shadow_phase_init_a;
                    phase_acc_b <= shadow_phase_init_b;
                end else if (shadow_ctrl_b_phase_adjust) begin
                    // Keep A phase-continuous while applying a signed B delta.
                    phase_acc_a <= phase_acc_a + shadow_step_a;
                    phase_acc_b <= phase_acc_b + shadow_step_b +
                                   shadow_phase_init_b;
                end else begin
                    // Tracking changes frequency without reloading phase.
                    phase_acc_a <= phase_acc_a + shadow_step_a;
                    phase_acc_b <= phase_acc_b + shadow_step_b;
                end
            end else begin
                if (!commit_pending && commit_trigger) begin
                    // COMMIT_SEQ is observed after its own BRAM read. Delay
                    // application until the next scan has refreshed all fields.
                    pending_commit_seq <= shadow_commit_seq;
                    commit_pending     <= 1'b1;
                end

                if (run_enabled) begin
                    // Normal DDS operation advances every DAC clock.
                    phase_acc_a <= phase_acc_a + run_step_a;
                    phase_acc_b <= phase_acc_b + run_step_b;
                end else begin
                    // Stopped state has a deterministic restart origin.
                    phase_acc_a <= 32'd0;
                    phase_acc_b <= 32'd0;
                end
            end

            // ----------------------------------------------------
            // 3. 最终 DAC 饱和截断 (分开赋值给 A 和 B)
            // ----------------------------------------------------
            // Keep the DDS arithmetic on the rising-edge pipeline. The output
            // pins are updated separately on the falling edge, preserving a
            // full cycle for this path while presenting data before WRT/CLK.
            if (!run_enabled || (apply_commit && !shadow_ctrl_run)) begin
                dac_sample_a <= 14'd8192;
                dac_sample_b <= 14'd8192;
            end else begin
                if (dac_code_a < 16'sd0)           dac_sample_a <= 14'd0;
                else if (dac_code_a > 16'sd16383)  dac_sample_a <= 14'd16383;
                else                                dac_sample_a <= dac_code_a[13:0];

                if (dac_code_b < 16'sd0)           dac_sample_b <= 14'd0;
                else if (dac_code_b > 16'sd16383)  dac_sample_b <= 14'd16383;
                else                                dac_sample_b <= dac_code_b[13:0];
            end
        end
    end

    // AD9767 latches data and updates on the WRT/CLK rising edge. Updating
    // data on the preceding falling edge provides a half-cycle setup window.
    always_ff @(negedge clk or negedge rst_n) begin
        if (!rst_n) begin
            da_data_a <= 14'd8192;
            da_data_b <= 14'd8192;
        end else begin
            da_data_a <= dac_sample_a;
            da_data_b <= dac_sample_b;
        end
    end


    ila_0 ila_debug
 (
    .clk    (clk),             // ！！！极度重要：必须换成 125MHz 的 DAC 采样时钟
    .probe0 (run_enabled),          // 1 bit
    .probe1 (phase_acc_a[31:20]),   // 12 bit
    .probe2 (dac_sample_a),         // 14 bit
    .probe3 (phase_acc_b[31:20]),   // 12 bit
    .probe4 (dac_sample_b)          // 14 bit
);
endmodule
