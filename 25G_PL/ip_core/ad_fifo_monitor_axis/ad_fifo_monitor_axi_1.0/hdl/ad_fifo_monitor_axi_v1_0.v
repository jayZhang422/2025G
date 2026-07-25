`timescale 1ns / 1ps

module ad_fifo_monitor_axi_v1_0 #(
    parameter integer C_ad_fifo_monitor_axi_DATA_WIDTH = 32,
    parameter integer C_ad_fifo_monitor_axi_ADDR_WIDTH = 7
) (
    input  wire adc_clk,
    input  wire sample_valid,
    input  wire fifo_write,
    input  wire fifo_prog_full,
    input  wire fifo_full,
    input  wire fifo_wr_rst_busy,
    input  wire fifo_rd_rst_busy,
    input  wire axis_tvalid,
    input  wire axis_tready,
    input  wire axis_tlast,

    input  wire ad_fifo_monitor_axi_aclk,
    input  wire ad_fifo_monitor_axi_aresetn,
    input  wire [C_ad_fifo_monitor_axi_ADDR_WIDTH-1:0] ad_fifo_monitor_axi_awaddr,
    input  wire [2:0] ad_fifo_monitor_axi_awprot,
    input  wire ad_fifo_monitor_axi_awvalid,
    output wire ad_fifo_monitor_axi_awready,
    input  wire [C_ad_fifo_monitor_axi_DATA_WIDTH-1:0] ad_fifo_monitor_axi_wdata,
    input  wire [(C_ad_fifo_monitor_axi_DATA_WIDTH/8)-1:0] ad_fifo_monitor_axi_wstrb,
    input  wire ad_fifo_monitor_axi_wvalid,
    output wire ad_fifo_monitor_axi_wready,
    output wire [1:0] ad_fifo_monitor_axi_bresp,
    output wire ad_fifo_monitor_axi_bvalid,
    input  wire ad_fifo_monitor_axi_bready,
    input  wire [C_ad_fifo_monitor_axi_ADDR_WIDTH-1:0] ad_fifo_monitor_axi_araddr,
    input  wire [2:0] ad_fifo_monitor_axi_arprot,
    input  wire ad_fifo_monitor_axi_arvalid,
    output wire ad_fifo_monitor_axi_arready,
    output wire [C_ad_fifo_monitor_axi_DATA_WIDTH-1:0] ad_fifo_monitor_axi_rdata,
    output wire [1:0] ad_fifo_monitor_axi_rresp,
    output wire ad_fifo_monitor_axi_rvalid,
    input  wire ad_fifo_monitor_axi_rready
);

    ad_fifo_monitor_axi_v1_0_ad_fifo_monitor_axi #(
        .C_S_AXI_DATA_WIDTH(C_ad_fifo_monitor_axi_DATA_WIDTH),
        .C_S_AXI_ADDR_WIDTH(C_ad_fifo_monitor_axi_ADDR_WIDTH)
    ) monitor_axi_inst (
        .adc_clk(adc_clk),
        .sample_valid(sample_valid),
        .fifo_write(fifo_write),
        .fifo_prog_full(fifo_prog_full),
        .fifo_full(fifo_full),
        .fifo_wr_rst_busy(fifo_wr_rst_busy),
        .fifo_rd_rst_busy(fifo_rd_rst_busy),
        .axis_tvalid(axis_tvalid),
        .axis_tready(axis_tready),
        .axis_tlast(axis_tlast),
        .S_AXI_ACLK(ad_fifo_monitor_axi_aclk),
        .S_AXI_ARESETN(ad_fifo_monitor_axi_aresetn),
        .S_AXI_AWADDR(ad_fifo_monitor_axi_awaddr),
        .S_AXI_AWPROT(ad_fifo_monitor_axi_awprot),
        .S_AXI_AWVALID(ad_fifo_monitor_axi_awvalid),
        .S_AXI_AWREADY(ad_fifo_monitor_axi_awready),
        .S_AXI_WDATA(ad_fifo_monitor_axi_wdata),
        .S_AXI_WSTRB(ad_fifo_monitor_axi_wstrb),
        .S_AXI_WVALID(ad_fifo_monitor_axi_wvalid),
        .S_AXI_WREADY(ad_fifo_monitor_axi_wready),
        .S_AXI_BRESP(ad_fifo_monitor_axi_bresp),
        .S_AXI_BVALID(ad_fifo_monitor_axi_bvalid),
        .S_AXI_BREADY(ad_fifo_monitor_axi_bready),
        .S_AXI_ARADDR(ad_fifo_monitor_axi_araddr),
        .S_AXI_ARPROT(ad_fifo_monitor_axi_arprot),
        .S_AXI_ARVALID(ad_fifo_monitor_axi_arvalid),
        .S_AXI_ARREADY(ad_fifo_monitor_axi_arready),
        .S_AXI_RDATA(ad_fifo_monitor_axi_rdata),
        .S_AXI_RRESP(ad_fifo_monitor_axi_rresp),
        .S_AXI_RVALID(ad_fifo_monitor_axi_rvalid),
        .S_AXI_RREADY(ad_fifo_monitor_axi_rready)
    );

endmodule
