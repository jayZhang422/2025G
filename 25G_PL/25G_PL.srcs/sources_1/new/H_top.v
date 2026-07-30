`timescale 1ns / 1ps

// PL acquisition logic plus BRAM-controlled DDS/DAC output IP.
// This file intentionally uses Verilog-2001 syntax because its extension is .v.
module H_top (
    input  wire        i_clk_50m,
    input  wire        i_clk_100m,
    input  wire        i_clk_dac,
    input  wire        i_rst,
    input  wire [11:0] i_ad_data,

    output wire        o_ad_clk,
    output wire [13:0] o_da_data,
    output wire        o_da_wrt,
    output wire        o_da_clk,
    output wire [13:0] o_da_data_b,
    output wire        o_da_wrt_b,
    output wire        o_da_clk_b,

    output wire [15:0] m_axis_tdata,
    output wire        m_axis_tvalid,
    input  wire        m_axis_tready,
    output wire        m_axis_tlast,

    output wire        iq_clk_adc,
    output wire [11:0] iq_adc_raw,
    output wire        iq_sample_valid,

    output wire        fifo_mon_write,
    output wire        fifo_mon_prog_full,
    output wire        fifo_mon_full,
    output wire        fifo_mon_wr_rst_busy,
    output wire        fifo_mon_rd_rst_busy,

    output wire [31:0] bram_addr,
    output wire        bram_en,
    output wire [3:0]  bram_we,
    input  wire [31:0] bram_dout
);

    wire        w_ad_clk;
    wire        w_ad_phase;
    wire        w_fifo_empty;
    wire        w_fifo_rd_en;
    wire [15:0] w_fifo_data;
    wire        w_dac_clk_a_forwarded;
    wire        w_dac_clk_b_forwarded;
    wire        w_dac_wrt_a_forwarded;
    wire        w_dac_wrt_b_forwarded;
    wire [11:0] w_sine_addr_a;
    wire [11:0] w_sine_addr_b;
    wire [11:0] w_triangle_addr_a;
    wire [11:0] w_triangle_addr_b;
    wire [13:0] w_sine_data_a;
    wire [13:0] w_sine_data_b;
    wire [13:0] w_triangle_data_a;
    wire [13:0] w_triangle_data_b;
    reg  [11:0] w_tlast_cnt;

    assign o_ad_clk = w_ad_clk;
    assign iq_clk_adc = w_ad_phase;

    // Forward CLK and WRT through ODDR output resources so their pin delay
    // matches the falling-edge DAC data registers.
    ODDR #(.DDR_CLK_EDGE("OPPOSITE_EDGE")) dac_clk_a_forward (
        .C  (i_clk_dac), .CE(1'b1), .D1(1'b1), .D2(1'b0),
        .Q  (w_dac_clk_a_forwarded), .R(~i_rst), .S(1'b0)
    );
    ODDR #(.DDR_CLK_EDGE("OPPOSITE_EDGE")) dac_clk_b_forward (
        .C  (i_clk_dac), .CE(1'b1), .D1(1'b1), .D2(1'b0),
        .Q  (w_dac_clk_b_forwarded), .R(~i_rst), .S(1'b0)
    );
    ODDR #(.DDR_CLK_EDGE("OPPOSITE_EDGE")) dac_wrt_a_forward (
        .C  (i_clk_dac), .CE(1'b1), .D1(1'b1), .D2(1'b0),
        .Q  (w_dac_wrt_a_forwarded), .R(~i_rst), .S(1'b0)
    );
    ODDR #(.DDR_CLK_EDGE("OPPOSITE_EDGE")) dac_wrt_b_forward (
        .C  (i_clk_dac), .CE(1'b1), .D1(1'b1), .D2(1'b0),
        .Q  (w_dac_wrt_b_forwarded), .R(~i_rst), .S(1'b0)
    );
    assign o_da_clk   = w_dac_clk_a_forwarded;
    assign o_da_wrt   = w_dac_wrt_a_forwarded;
    assign o_da_clk_b = w_dac_clk_b_forwarded;
    assign o_da_wrt_b = w_dac_wrt_b_forwarded;

    // The AD/FIFO IP no longer owns the clock wizard. Keep the dedicated
    // 0-degree output on the ADC pin and use the phase-shifted output for
    // capture and the FIFO write clock.
    PLL_AD ad_clk (
        .clk_pll_ad    (w_ad_clk),
        .clk_pll_phase (w_ad_phase),
        .resetn        (i_rst),
        .locked        (),
        .clk_sys       (i_clk_50m)
    );

    ad_fifo_wrapper_0 u_ad_fifo (
        .rst_n     (i_rst),
        .clk_phase (w_ad_phase),
        .adc_din   (i_ad_data),
        .rd_clk    (i_clk_100m),
        .rd_en     (w_fifo_rd_en),
        .dout      (w_fifo_data),
        .empty     (w_fifo_empty),
        .adc_raw        (iq_adc_raw),
        .sample_valid   (iq_sample_valid),
        .mon_fifo_write (fifo_mon_write),
        .mon_prog_full  (fifo_mon_prog_full),
        .mon_fifo_full  (fifo_mon_full),
        .mon_wr_rst_busy(fifo_mon_wr_rst_busy),
        .mon_rd_rst_busy(fifo_mon_rd_rst_busy)
    );

    assign w_fifo_rd_en  = ~w_fifo_empty & m_axis_tready;
    assign m_axis_tdata  = w_fifo_data;
    assign m_axis_tvalid = ~w_fifo_empty;

    // AXIS handshake and TLAST must remain in the FCLK_CLK0 domain.
    always @(posedge i_clk_100m or negedge i_rst) begin
        if (!i_rst)
            w_tlast_cnt <= 12'd0;
        else if (m_axis_tvalid && m_axis_tready)
            w_tlast_cnt <= (w_tlast_cnt == 12'd4095) ? 12'd0 : w_tlast_cnt + 1'b1;
    end

    assign m_axis_tlast = (w_tlast_cnt == 12'd4095) && m_axis_tvalid;

    // DDS_DAC keeps the waveform memories external. These four instances
    // preserve the prior independent A/B sine and triangle ROM topology.
    blk_rom_sine sine_a (
        .clka  (i_clk_dac), .ena(1'b1),
        .addra (w_sine_addr_a), .douta (w_sine_data_a)
    );
    blk_rom_triangle triangle_a (
        .clka  (i_clk_dac), .ena(1'b1),
        .addra (w_triangle_addr_a), .douta (w_triangle_data_a)
    );
    blk_rom_sine sine_b (
        .clka  (i_clk_dac), .ena(1'b1),
        .addra (w_sine_addr_b), .douta (w_sine_data_b)
    );
    blk_rom_triangle triangle_b (
        .clka  (i_clk_dac), .ena(1'b1),
        .addra (w_triangle_addr_b), .douta (w_triangle_data_b)
    );

    DDS_DAC u_dac_dds (
        .clk             (i_clk_dac),
        .rst_n           (i_rst),
        .bram_addr       (bram_addr),
        .bram_dout       (bram_dout),
        .bram_en         (bram_en),
        .bram_we         (bram_we),
        .sine_addr_a     (w_sine_addr_a),
        .sine_data_a     (w_sine_data_a),
        .sine_addr_b     (w_sine_addr_b),
        .sine_data_b     (w_sine_data_b),
        .triangle_addr_a (w_triangle_addr_a),
        .triangle_data_a (w_triangle_data_a),
        .triangle_addr_b (w_triangle_addr_b),
        .triangle_data_b (w_triangle_data_b),
        .arb_addr_a      (),
        .arb_data_a      (14'd8192),
        .arb_addr_b      (),
        .arb_data_b      (14'd8192),
        .da_data_a       (o_da_data),
        .da_data_b       (o_da_data_b),
        .da_wrt_a        (),
        .da_wrt_b        ()
    );
 
endmodule
