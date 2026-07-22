`timescale 1ns / 1ps

// =============================================================================
// DAC_DDS_Output
//
// Package-friendly DDS/DAC core.
//
// Control BRAM map, word addressed through bram_addr:
//   0x00: Channel A waveform (0 sine, 1 triangle, 2 arbitrary)
//   0x04: Channel A frequency step
//   0x08: Channel A initial phase
//   0x0C: Channel A amplitude [13:0]
//   0x10: Channel B waveform (0 sine, 1 triangle, 2 arbitrary)
//   0x14: Channel B frequency step
//   0x18: Channel B initial phase
//   0x1C: Channel B amplitude [13:0]
//   0x20: System control
//         Bit0: RUN enable
//         Bit1: PHASE_RELOAD enable
//         Bit2: B phase adjust, use 0x18 as signed delta instead of absolute phase
//   0x24: Atomic commit sequence COMMIT_SEQ
//
// Waveform memories are intentionally outside this module. In a Vivado user IP,
// connect these ports to Block Memory Generator / blk_mem_gen / blk_rom IPs in
// Block Design. For arbitrary waveform output, PS can update the external
// waveform RAM without repackaging this DDS core.
// =============================================================================

module DAC_DDS_Output #(
    parameter int PHASE_WIDTH = 32,
    parameter int ADDR_WIDTH  = 12,
    parameter int DATA_WIDTH  = 14
) (
    input  logic                    clk,
    input  logic                    rst_n,

    // BRAM control table read interface
    output logic [31:0]             bram_addr,
    input  logic [31:0]             bram_dout,
    output logic                    bram_en,
    output logic [3:0]              bram_we,

    // External waveform memories, usually provided by Block Design.
    output logic [ADDR_WIDTH-1:0]   sine_addr_a,
    input  logic [DATA_WIDTH-1:0]   sine_data_a,
    output logic [ADDR_WIDTH-1:0]   sine_addr_b,
    input  logic [DATA_WIDTH-1:0]   sine_data_b,

    output logic [ADDR_WIDTH-1:0]   triangle_addr_a,
    input  logic [DATA_WIDTH-1:0]   triangle_data_a,
    output logic [ADDR_WIDTH-1:0]   triangle_addr_b,
    input  logic [DATA_WIDTH-1:0]   triangle_data_b,

    output logic [ADDR_WIDTH-1:0]   arb_addr_a,
    input  logic [DATA_WIDTH-1:0]   arb_data_a,
    output logic [ADDR_WIDTH-1:0]   arb_addr_b,
    input  logic [DATA_WIDTH-1:0]   arb_data_b,

    // AD9767 dual-channel DAC physical interface
    output logic [DATA_WIDTH-1:0]   da_data_a,
    output logic [DATA_WIDTH-1:0]   da_data_b,
    output logic                    da_wrt_a,
    output logic                    da_wrt_b
);

    localparam logic [1:0] WAVE_SINE     = 2'd0;
    localparam logic [1:0] WAVE_TRIANGLE = 2'd1;
    localparam logic [1:0] WAVE_ARB      = 2'd2;

    localparam logic [DATA_WIDTH-1:0] DAC_MID   = 14'd8192;
    localparam logic [DATA_WIDTH-1:0] DAC_MAX   = 14'd16383;

    logic [3:0] cfg_sel;
    logic [3:0] cfg_sel_d;

    logic [1:0]  shadow_wave_a;
    logic [1:0]  shadow_wave_b;
    logic [31:0] shadow_step_a;
    logic [31:0] shadow_step_b;
    logic [31:0] shadow_phase_init_a;
    logic [31:0] shadow_phase_init_b;
    logic [13:0] shadow_amp_a;
    logic [13:0] shadow_amp_b;
    logic        shadow_ctrl_run;
    logic        shadow_ctrl_reload;
    logic        shadow_ctrl_b_phase_adjust;
    logic [31:0] shadow_commit_seq;

    logic [1:0]  run_wave_a;
    logic [1:0]  run_wave_b;
    logic [31:0] run_step_a;
    logic [31:0] run_step_b;
    logic [13:0] run_amp_a;
    logic [13:0] run_amp_b;
    logic        run_enabled;

    logic [PHASE_WIDTH-1:0] phase_acc_a;
    logic [PHASE_WIDTH-1:0] phase_acc_b;

    logic [DATA_WIDTH-1:0] waveform_data_a;
    logic [DATA_WIDTH-1:0] waveform_data_b;
    logic signed [14:0]    centered_a;
    logic signed [14:0]    centered_b;
    logic signed [29:0]    scaled_a;
    logic signed [29:0]    scaled_b;
    logic signed [15:0]    dac_code_a;
    logic signed [15:0]    dac_code_b;
    logic [DATA_WIDTH-1:0] dac_sample_a;
    logic [DATA_WIDTH-1:0] dac_sample_b;

    logic [31:0] last_commit_seq;
    logic [31:0] pending_commit_seq;
    logic        commit_pending;

    wire commit_trigger = (shadow_commit_seq != last_commit_seq);
    wire apply_commit   = commit_pending && (cfg_sel_d == 4'd9);

    assign bram_en   = 1'b1;
    assign bram_we   = 4'b0;
    assign bram_addr = {26'd0, cfg_sel, 2'b0};

    assign da_wrt_a = clk;
    assign da_wrt_b = clk;

    assign sine_addr_a     = phase_acc_a[PHASE_WIDTH-1 -: ADDR_WIDTH];
    assign sine_addr_b     = phase_acc_b[PHASE_WIDTH-1 -: ADDR_WIDTH];
    assign triangle_addr_a = phase_acc_a[PHASE_WIDTH-1 -: ADDR_WIDTH];
    assign triangle_addr_b = phase_acc_b[PHASE_WIDTH-1 -: ADDR_WIDTH];
    assign arb_addr_a      = phase_acc_a[PHASE_WIDTH-1 -: ADDR_WIDTH];
    assign arb_addr_b      = phase_acc_b[PHASE_WIDTH-1 -: ADDR_WIDTH];

    always_comb begin
        unique case (run_wave_a)
            WAVE_TRIANGLE: waveform_data_a = triangle_data_a;
            WAVE_ARB:      waveform_data_a = arb_data_a;
            default:       waveform_data_a = sine_data_a;
        endcase

        unique case (run_wave_b)
            WAVE_TRIANGLE: waveform_data_b = triangle_data_b;
            WAVE_ARB:      waveform_data_b = arb_data_b;
            default:       waveform_data_b = sine_data_b;
        endcase

        centered_a = $signed({1'b0, waveform_data_a}) - 15'sd8192;
        centered_b = $signed({1'b0, waveform_data_b}) - 15'sd8192;
        scaled_a   = centered_a * $signed({1'b0, run_amp_a});
        scaled_b   = centered_b * $signed({1'b0, run_amp_b});
        dac_code_a = (scaled_a >>> 14) + 16'sd8192;
        dac_code_b = (scaled_b >>> 14) + 16'sd8192;
    end

    always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            cfg_sel                    <= 4'd0;
            cfg_sel_d                  <= 4'd0;
            last_commit_seq            <= 32'd0;
            pending_commit_seq         <= 32'd0;
            commit_pending             <= 1'b0;

            shadow_wave_a              <= WAVE_SINE;
            shadow_wave_b              <= WAVE_SINE;
            shadow_step_a              <= 32'd0;
            shadow_step_b              <= 32'd0;
            shadow_phase_init_a        <= 32'd0;
            shadow_phase_init_b        <= 32'd0;
            shadow_amp_a               <= 14'd0;
            shadow_amp_b               <= 14'd0;
            shadow_ctrl_run            <= 1'b0;
            shadow_ctrl_reload         <= 1'b0;
            shadow_ctrl_b_phase_adjust <= 1'b0;
            shadow_commit_seq          <= 32'd0;

            run_wave_a                 <= WAVE_SINE;
            run_wave_b                 <= WAVE_SINE;
            run_step_a                 <= 32'd0;
            run_step_b                 <= 32'd0;
            run_amp_a                  <= 14'd0;
            run_amp_b                  <= 14'd0;
            run_enabled                <= 1'b0;

            phase_acc_a                <= '0;
            phase_acc_b                <= '0;
            dac_sample_a               <= DAC_MID;
            dac_sample_b               <= DAC_MID;
        end else begin
            cfg_sel_d <= cfg_sel;
            cfg_sel   <= (cfg_sel == 4'd9) ? 4'd0 : cfg_sel + 1'b1;

            case (cfg_sel_d)
                4'd0: shadow_wave_a              <= bram_dout[1:0];
                4'd1: shadow_step_a              <= bram_dout;
                4'd2: shadow_phase_init_a        <= bram_dout;
                4'd3: shadow_amp_a               <= bram_dout[13:0];
                4'd4: shadow_wave_b              <= bram_dout[1:0];
                4'd5: shadow_step_b              <= bram_dout;
                4'd6: shadow_phase_init_b        <= bram_dout;
                4'd7: shadow_amp_b               <= bram_dout[13:0];
                4'd8: begin
                    shadow_ctrl_run              <= bram_dout[0];
                    shadow_ctrl_reload           <= bram_dout[1];
                    shadow_ctrl_b_phase_adjust   <= bram_dout[2];
                end
                4'd9: shadow_commit_seq          <= bram_dout;
                default: begin
                end
            endcase

            if (apply_commit) begin
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
                    phase_acc_a <= '0;
                    phase_acc_b <= '0;
                end else if (shadow_ctrl_reload) begin
                    phase_acc_a <= shadow_phase_init_a;
                    phase_acc_b <= shadow_phase_init_b;
                end else if (shadow_ctrl_b_phase_adjust) begin
                    phase_acc_a <= phase_acc_a + shadow_step_a;
                    phase_acc_b <= phase_acc_b + shadow_step_b + shadow_phase_init_b;
                end else begin
                    phase_acc_a <= phase_acc_a + shadow_step_a;
                    phase_acc_b <= phase_acc_b + shadow_step_b;
                end
            end else begin
                if (!commit_pending && commit_trigger) begin
                    pending_commit_seq <= shadow_commit_seq;
                    commit_pending     <= 1'b1;
                end

                if (run_enabled) begin
                    phase_acc_a <= phase_acc_a + run_step_a;
                    phase_acc_b <= phase_acc_b + run_step_b;
                end else begin
                    phase_acc_a <= '0;
                    phase_acc_b <= '0;
                end
            end

            if (!run_enabled || (apply_commit && !shadow_ctrl_run)) begin
                dac_sample_a <= DAC_MID;
                dac_sample_b <= DAC_MID;
            end else begin
                if (dac_code_a < 16'sd0)          dac_sample_a <= '0;
                else if (dac_code_a > 16'sd16383) dac_sample_a <= DAC_MAX;
                else                              dac_sample_a <= dac_code_a[DATA_WIDTH-1:0];

                if (dac_code_b < 16'sd0)          dac_sample_b <= '0;
                else if (dac_code_b > 16'sd16383) dac_sample_b <= DAC_MAX;
                else                              dac_sample_b <= dac_code_b[DATA_WIDTH-1:0];
            end
        end
    end

    always_ff @(negedge clk or negedge rst_n) begin
        if (!rst_n) begin
            da_data_a <= DAC_MID;
            da_data_b <= DAC_MID;
        end else begin
            da_data_a <= dac_sample_a;
            da_data_b <= dac_sample_b;
        end
    end

endmodule
