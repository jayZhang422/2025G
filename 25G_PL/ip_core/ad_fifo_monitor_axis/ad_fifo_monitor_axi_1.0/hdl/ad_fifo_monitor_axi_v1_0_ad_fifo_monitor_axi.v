`timescale 1ns / 1ps

module ad_fifo_monitor_axi_v1_0_ad_fifo_monitor_axi #(
    parameter integer C_S_AXI_DATA_WIDTH = 32,
    parameter integer C_S_AXI_ADDR_WIDTH = 7
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

    input  wire S_AXI_ACLK,
    input  wire S_AXI_ARESETN,
    input  wire [C_S_AXI_ADDR_WIDTH-1:0] S_AXI_AWADDR,
    input  wire [2:0] S_AXI_AWPROT,
    input  wire S_AXI_AWVALID,
    output wire S_AXI_AWREADY,
    input  wire [C_S_AXI_DATA_WIDTH-1:0] S_AXI_WDATA,
    input  wire [(C_S_AXI_DATA_WIDTH/8)-1:0] S_AXI_WSTRB,
    input  wire S_AXI_WVALID,
    output wire S_AXI_WREADY,
    output wire [1:0] S_AXI_BRESP,
    output wire S_AXI_BVALID,
    input  wire S_AXI_BREADY,
    input  wire [C_S_AXI_ADDR_WIDTH-1:0] S_AXI_ARADDR,
    input  wire [2:0] S_AXI_ARPROT,
    input  wire S_AXI_ARVALID,
    output wire S_AXI_ARREADY,
    output wire [C_S_AXI_DATA_WIDTH-1:0] S_AXI_RDATA,
    output wire [1:0] S_AXI_RRESP,
    output wire S_AXI_RVALID,
    input  wire S_AXI_RREADY
);

    localparam [31:0] CORE_VERSION = 32'h0001_0000;

    reg [C_S_AXI_ADDR_WIDTH-1:0] write_address;
    reg [C_S_AXI_DATA_WIDTH-1:0] write_data;
    reg [(C_S_AXI_DATA_WIDTH/8)-1:0] write_strobe;
    reg write_address_pending;
    reg write_data_pending;
    reg write_response_valid;
    reg read_response_valid;
    reg [C_S_AXI_DATA_WIDTH-1:0] read_response_data;
    reg snapshot_pulse;
    reg clear_sticky_pulse;

    wire snapshot_busy;
    wire snapshot_valid;
    wire clear_busy;
    wire [63:0] adc_sample_count;
    wire [63:0] fifo_write_count;
    wire [63:0] blocked_high_watermark_count;
    wire [63:0] blocked_reset_count;
    wire [63:0] axis_beat_count;
    wire [63:0] frame_count;
    wire [63:0] axis_stall_cycle_count;
    wire [63:0] last_frame_timestamp;
    wire write_blocked_sticky;
    wire fifo_full_sticky;
    wire status_prog_full;
    wire status_fifo_full;
    wire status_wr_rst_busy;
    wire status_rd_rst_busy;

    reg [31:0] read_data_mux;

    assign S_AXI_AWREADY = !write_address_pending && !write_response_valid;
    assign S_AXI_WREADY  = !write_data_pending && !write_response_valid;
    assign S_AXI_BRESP   = 2'b00;
    assign S_AXI_BVALID  = write_response_valid;
    assign S_AXI_ARREADY = !read_response_valid;
    assign S_AXI_RDATA   = read_response_data;
    assign S_AXI_RRESP   = 2'b00;
    assign S_AXI_RVALID  = read_response_valid;

    always @(posedge S_AXI_ACLK) begin
        if (!S_AXI_ARESETN) begin
            write_address         <= {C_S_AXI_ADDR_WIDTH{1'b0}};
            write_data            <= {C_S_AXI_DATA_WIDTH{1'b0}};
            write_strobe          <= {(C_S_AXI_DATA_WIDTH/8){1'b0}};
            write_address_pending <= 1'b0;
            write_data_pending    <= 1'b0;
            write_response_valid  <= 1'b0;
            snapshot_pulse        <= 1'b0;
            clear_sticky_pulse     <= 1'b0;
        end else begin
            snapshot_pulse    <= 1'b0;
            clear_sticky_pulse <= 1'b0;

            if (S_AXI_AWREADY && S_AXI_AWVALID) begin
                write_address         <= S_AXI_AWADDR;
                write_address_pending <= 1'b1;
            end
            if (S_AXI_WREADY && S_AXI_WVALID) begin
                write_data         <= S_AXI_WDATA;
                write_strobe       <= S_AXI_WSTRB;
                write_data_pending <= 1'b1;
            end

            if (!write_response_valid && write_address_pending && write_data_pending) begin
                if ((write_address[6:2] == 5'h00) && write_strobe[0]) begin
                    snapshot_pulse    <= write_data[0];
                    clear_sticky_pulse <= write_data[1];
                end
                write_address_pending <= 1'b0;
                write_data_pending    <= 1'b0;
                write_response_valid  <= 1'b1;
            end else if (write_response_valid && S_AXI_BREADY) begin
                write_response_valid <= 1'b0;
            end
        end
    end

    always @(*) begin
        case (S_AXI_ARADDR[6:2])
            5'h00: read_data_mux = 32'd0;
            5'h01: read_data_mux = {
                23'd0,
                status_rd_rst_busy,
                status_wr_rst_busy,
                status_fifo_full,
                status_prog_full,
                fifo_full_sticky,
                write_blocked_sticky,
                clear_busy,
                snapshot_valid,
                snapshot_busy
            };
            5'h02: read_data_mux = CORE_VERSION;
            5'h04: read_data_mux = adc_sample_count[31:0];
            5'h05: read_data_mux = adc_sample_count[63:32];
            5'h06: read_data_mux = fifo_write_count[31:0];
            5'h07: read_data_mux = fifo_write_count[63:32];
            5'h08: read_data_mux = blocked_high_watermark_count[31:0];
            5'h09: read_data_mux = blocked_high_watermark_count[63:32];
            5'h0A: read_data_mux = blocked_reset_count[31:0];
            5'h0B: read_data_mux = blocked_reset_count[63:32];
            5'h0C: read_data_mux = axis_beat_count[31:0];
            5'h0D: read_data_mux = axis_beat_count[63:32];
            5'h0E: read_data_mux = frame_count[31:0];
            5'h0F: read_data_mux = frame_count[63:32];
            5'h10: read_data_mux = axis_stall_cycle_count[31:0];
            5'h11: read_data_mux = axis_stall_cycle_count[63:32];
            5'h12: read_data_mux = last_frame_timestamp[31:0];
            5'h13: read_data_mux = last_frame_timestamp[63:32];
            default: read_data_mux = 32'd0;
        endcase
    end

    always @(posedge S_AXI_ACLK) begin
        if (!S_AXI_ARESETN) begin
            read_response_valid <= 1'b0;
            read_response_data  <= {C_S_AXI_DATA_WIDTH{1'b0}};
        end else begin
            if (S_AXI_ARREADY && S_AXI_ARVALID) begin
                read_response_valid <= 1'b1;
                read_response_data  <= read_data_mux;
            end else if (read_response_valid && S_AXI_RREADY) begin
                read_response_valid <= 1'b0;
            end
        end
    end

    ad_fifo_monitor monitor_core (
        .rst_n(S_AXI_ARESETN),
        .adc_clk(adc_clk),
        .axis_clk(S_AXI_ACLK),
        .sample_valid(sample_valid),
        .fifo_write(fifo_write),
        .fifo_prog_full(fifo_prog_full),
        .fifo_full(fifo_full),
        .fifo_wr_rst_busy(fifo_wr_rst_busy),
        .fifo_rd_rst_busy(fifo_rd_rst_busy),
        .axis_tvalid(axis_tvalid),
        .axis_tready(axis_tready),
        .axis_tlast(axis_tlast),
        .snapshot(snapshot_pulse),
        .clear_sticky(clear_sticky_pulse),
        .snapshot_busy(snapshot_busy),
        .snapshot_valid(snapshot_valid),
        .clear_busy(clear_busy),
        .adc_sample_count(adc_sample_count),
        .fifo_write_count(fifo_write_count),
        .blocked_high_watermark_count(blocked_high_watermark_count),
        .blocked_reset_count(blocked_reset_count),
        .axis_beat_count(axis_beat_count),
        .frame_count(frame_count),
        .axis_stall_cycle_count(axis_stall_cycle_count),
        .last_frame_timestamp(last_frame_timestamp),
        .write_blocked_sticky(write_blocked_sticky),
        .fifo_full_sticky(fifo_full_sticky),
        .status_prog_full(status_prog_full),
        .status_fifo_full(status_fifo_full),
        .status_wr_rst_busy(status_wr_rst_busy),
        .status_rd_rst_busy(status_rd_rst_busy)
    );

endmodule
