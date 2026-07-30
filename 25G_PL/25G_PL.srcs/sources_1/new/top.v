`timescale 1ns / 1ps

module top (
    input  wire        i_clk_50m,
    input  wire        i_rst,
    input  wire [2:0]  pl_key_i,
    input  wire        i_hmi_uart_rx,
    output wire        o_hmi_uart_tx,
    input  wire [11:0] i_ad_data,
    output wire        o_ad_clk,
    inout  wire [14:0] DDR_addr,
    inout  wire [2:0]  DDR_ba,
    inout  wire        DDR_cas_n,
    inout  wire        DDR_ck_n,
    inout  wire        DDR_ck_p,
    inout  wire        DDR_cke,
    inout  wire        DDR_cs_n,
    inout  wire [3:0]  DDR_dm,
    inout  wire [31:0] DDR_dq,
    inout  wire [3:0]  DDR_dqs_n,
    inout  wire [3:0]  DDR_dqs_p,
    inout  wire        DDR_odt,
    inout  wire        DDR_ras_n,
    inout  wire        DDR_reset_n,
    inout  wire        DDR_we_n,
    inout  wire        FIXED_IO_ddr_vrn,
    inout  wire        FIXED_IO_ddr_vrp,
    inout  wire [53:0] FIXED_IO_mio,
    inout  wire        FIXED_IO_ps_clk,
    inout  wire        FIXED_IO_ps_porb,
    inout  wire        FIXED_IO_ps_srstb
);
    wire        clk_dac;
    wire        fclk;
    wire [15:0] adc_tdata;
    wire        adc_tvalid;
    wire        adc_tready;
    wire        adc_tlast;
    wire [15:0] fir_tdata;
    wire        fir_tvalid;
    wire        fir_tready;
    wire        fir_tlast;
    wire [15:0] ddc_tdata;
    wire        ddc_tvalid;
    wire        ddc_tready;
    wire        ddc_tlast;
    wire        iq_clk_adc;
    wire [11:0] iq_adc_raw;
    wire        iq_sample_valid;
    wire        fifo_mon_write;
    wire        fifo_mon_prog_full;
    wire        fifo_mon_full;
    wire        fifo_mon_wr_rst_busy;
    wire        fifo_mon_rd_rst_busy;
    wire [31:0] bram_addr;
    wire [31:0] bram_dout;
    wire        bram_en;
    wire [3:0]  bram_we;

    assign ddc_tready = 1'b0;

    H_top u_h_top (
        .i_clk_50m     (i_clk_50m),
        .i_clk_100m    (fclk),
        .i_clk_dac     (clk_dac),
        .i_rst         (i_rst),
        .i_ad_data     (i_ad_data),
        .o_ad_clk      (o_ad_clk),
        .o_da_data     (),
        .o_da_wrt      (),
        .o_da_clk      (),
        .o_da_data_b   (),
        .o_da_wrt_b    (),
        .o_da_clk_b    (),
        .m_axis_tdata  (adc_tdata),
        .m_axis_tvalid (adc_tvalid),
        .m_axis_tready (adc_tready),
        .m_axis_tlast  (adc_tlast),
        .iq_clk_adc    (iq_clk_adc),
        .iq_adc_raw    (iq_adc_raw),
        .iq_sample_valid (iq_sample_valid),
        .fifo_mon_write      (fifo_mon_write),
        .fifo_mon_prog_full  (fifo_mon_prog_full),
        .fifo_mon_full       (fifo_mon_full),
        .fifo_mon_wr_rst_busy(fifo_mon_wr_rst_busy),
        .fifo_mon_rd_rst_busy(fifo_mon_rd_rst_busy),
        .bram_addr     (bram_addr),
        .bram_en       (bram_en),
        .bram_we       (bram_we),
        .bram_dout     (bram_dout)
    );

    adc_fir_axis u_adc_fir (
        .aclk          (fclk),
        .aresetn       (i_rst),
        .s_axis_tdata  (adc_tdata),
        .s_axis_tvalid (adc_tvalid),
        .s_axis_tready (adc_tready),
        .m_axis_tdata  (fir_tdata),
        .m_axis_tvalid (fir_tvalid),
        .m_axis_tready (fir_tready),
        .m_axis_tlast  (fir_tlast)
    );

    system_wrapper u_system (
        .ADC_STREAM_IN_tdata  (fir_tdata),
        .ADC_STREAM_IN_tlast  (fir_tlast),
        .ADC_STREAM_IN_tready (fir_tready),
        .ADC_STREAM_IN_tvalid (fir_tvalid),
        .BRAM_DATA_addr       (bram_addr),
        .BRAM_DATA_clk        (clk_dac),
        .BRAM_DATA_din        (32'd0),
        .BRAM_DATA_dout       (bram_dout),
        .BRAM_DATA_en         (bram_en),
        .BRAM_DATA_rst        (~i_rst),
        .BRAM_DATA_we         (bram_we),
        .DDR_addr             (DDR_addr),
        .DDR_ba               (DDR_ba),
        .DDR_cas_n            (DDR_cas_n),
        .DDR_ck_n             (DDR_ck_n),
        .DDR_ck_p             (DDR_ck_p),
        .DDR_cke              (DDR_cke),
        .DDR_cs_n             (DDR_cs_n),
        .DDR_dm               (DDR_dm),
        .DDR_dq               (DDR_dq),
        .DDR_dqs_n            (DDR_dqs_n),
        .DDR_dqs_p            (DDR_dqs_p),
        .DDR_odt              (DDR_odt),
        .DDR_ras_n            (DDR_ras_n),
        .DDR_reset_n          (DDR_reset_n),
        .DDR_we_n             (DDR_we_n),
        .FCLK_CLK0_0          (fclk),
        .FIXED_IO_ddr_vrn     (FIXED_IO_ddr_vrn),
        .FIXED_IO_ddr_vrp     (FIXED_IO_ddr_vrp),
        .FIXED_IO_mio         (FIXED_IO_mio),
        .FIXED_IO_ps_clk      (FIXED_IO_ps_clk),
        .FIXED_IO_ps_porb     (FIXED_IO_ps_porb),
        .FIXED_IO_ps_srstb    (FIXED_IO_ps_srstb),
        .PL_HMI_UART_rxd      (i_hmi_uart_rx),
        .PL_HMI_UART_txd      (o_hmi_uart_tx),
        .adc_clk_0            (iq_clk_adc),
        .clk_dac              (clk_dac),
        .fifo_full_0          (fifo_mon_full),
        .fifo_prog_full_0     (fifo_mon_prog_full),
        .fifo_rd_rst_busy_0   (fifo_mon_rd_rst_busy),
        .fifo_wr_rst_busy_0   (fifo_mon_wr_rst_busy),
        .fifo_write_0         (fifo_mon_write),
        .i_adc_raw_0          (iq_adc_raw),
        .i_adc_raw_1          (iq_adc_raw),
        .i_sample_valid_0     (iq_sample_valid),
        .i_sample_valid_1     (iq_sample_valid),
        .m_ddc_stream_axis_tdata (ddc_tdata),
        .m_ddc_stream_axis_tlast (ddc_tlast),
        .m_ddc_stream_axis_tready(ddc_tready),
        .m_ddc_stream_axis_tvalid(ddc_tvalid),
        .sample_valid_0       (iq_sample_valid),
        .axis_tlast_0         (adc_tlast),
        .axis_tready_0        (adc_tready),
        .axis_tvalid_0        (adc_tvalid),
        .pl_key_i             (pl_key_i),
        .rst_n_0              (i_rst)
    );
    ila_0 ila_debug (
	.clk(fclk), // input wire clk
	.probe0(fir_tlast), // input wire [0:0]  probe0  
	.probe1(fir_tdata[11:0]), // input wire [11:0]  probe1 
	.probe2(fir_tready), // input wire [0:0]  probe2 
	.probe3(fir_tvalid), // input wire [0:0]  probe3 
	.probe4(i_rst) // input wire [0:0]  probe4
);

endmodule
