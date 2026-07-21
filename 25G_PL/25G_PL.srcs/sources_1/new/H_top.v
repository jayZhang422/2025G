`timescale 1ns / 1ps

// PL acquisition logic plus BRAM-controlled AD9767 output.
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

    output wire [31:0] bram_addr,
    output wire        bram_en,
    output wire [3:0]  bram_we,
    input  wire [31:0] bram_dout
);

    wire        w_ad_clk;
    wire        w_ad_phase;
    wire        w_ad_valid;
    wire [11:0] w_ad_data;
    wire        w_fifo_empty;
    wire        w_fifo_rd_en;
    wire [15:0] w_fifo_data;
    wire        w_rst_safe;
    wire        w_dac_clk_a_forwarded;
    wire        w_dac_clk_b_forwarded;
    wire        w_dac_wrt_a_forwarded;
    wire        w_dac_wrt_b_forwarded;
    wire        w_dac_wrt_a_unused;
    wire        w_dac_wrt_b_unused;
    reg  [11:0] w_tlast_cnt;

    assign w_rst_safe = i_rst & w_ad_valid;
    assign o_ad_clk   = w_ad_clk;

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

    ad9226 u_ad9226 (
        .clk           (i_clk_50m),
        .rst_ad        (i_rst),
        .din           (i_ad_data),
        .clk_ad        (w_ad_clk),
        .dout          (w_ad_data),
        .ad_out_valid  (w_ad_valid),
        .clk_ad_deg    (w_ad_phase)
    );

    fifo u_adc_fifo (
        .rst       (w_rst_safe),
        .wr_clk    (w_ad_phase),
        .wr_en     (w_ad_valid),
        .din       (w_ad_data),
        .rd_clk    (i_clk_100m),
        .rd_en     (w_fifo_rd_en),
        .dout      (w_fifo_data),
        .empty     (w_fifo_empty)
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

    assign m_axis_tlast = (w_tlast_cnt == 12'd4095) &&
                          m_axis_tvalid ;

    ad9767 u_ad9767 (
        .clk       (i_clk_dac),
        .rst_n     (i_rst),
        .bram_addr (bram_addr),
        .bram_dout (bram_dout),
        .bram_en   (bram_en),
        .bram_we   (bram_we),
        .da_data_a (o_da_data),
        .da_data_b (o_da_data_b),
        .da_wrt_a  (w_dac_wrt_a_unused),
        .da_wrt_b  (w_dac_wrt_b_unused)
    );


endmodule
