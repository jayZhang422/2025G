`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Module Name: fifo
// Description: Dual-clock FIFO wrapper for 12-bit ADC to 16-bit data transfer.
//              Includes safe reset timing and read/write interlocking.
//              Clock domains are determined by the instantiating module (H_top.v):
//                wr_clk = 5.12MHz (ADC sampling phase clock, clk_ad_deg)
//                rd_clk = 100MHz  (PS FCLK_CLK0, shared with AXI Stream domain)
//////////////////////////////////////////////////////////////////////////////////

module fifo(
    input  wire        rst,         // Active-low asynchronous system reset

    // Write Domain (5.12MHz, ADC sampling phase clock)
    input  wire        wr_clk,      // ADC sampling clock (clk_ad_deg)
    input  wire        wr_en,       // ADC data valid signal (ad_out_valid)
    input  wire [11:0] din,         // 12-bit raw ADC data input

    // Read Domain (100MHz, PS FCLK_CLK0)
    input  wire        rd_clk,      // Read side clock
    input  wire        rd_en,       // Read enable from downstream consumer (e.g. AXI Stream logic)
    output wire [15:0] dout,        // 16-bit FIFO output data to downstream consumer
    output wire        empty,       // Legacy low-watermark flag (mapped to prog_empty)

    // Read-only monitor taps. These outputs never participate in FIFO control.
    output wire        mon_write_en,
    output wire        mon_prog_full,
    output wire        mon_fifo_full,
    output wire        mon_wr_rst_busy,
    output wire        mon_rd_rst_busy
);

    // Internal Signals
    wire [15:0] ad_data_in;
    wire        fifo_full;
    wire        prog_full;
    wire        wr_rst_busy;
    wire        prog_empty;
    wire        rd_rst_busy;

    // Write Control: prog_full is the normal high-watermark guard; full is
    // also checked so the monitor reports only writes accepted by the FIFO.
    wire write_allow = (!wr_rst_busy) && (!prog_full) && (!fifo_full);
    wire write_en    = wr_en && write_allow;

    // Read Control: Block reads if FIFO is empty or resetting
    wire read_allow  = (!rd_rst_busy) && (!prog_empty);
    wire read_en     = rd_en && read_allow;

    assign empty           = prog_empty;
    assign mon_write_en    = write_en;
    assign mon_prog_full   = prog_full;
    assign mon_fifo_full   = fifo_full;
    assign mon_wr_rst_busy = wr_rst_busy;
    assign mon_rd_rst_busy = rd_rst_busy;

    // Do not release the dual-clock FIFO until its PS/FCLK read clock has
    // actually started.  FPGA configuration can finish before FCLK0 is
    // running; releasing reset in that interval leaves FIFO Generator's
    // reset-busy outputs asserted indefinitely.
    reg       rd_clock_seen = 1'b0;
    (* ASYNC_REG = "TRUE" *) reg [1:0] rd_clock_seen_sync = 2'b00;

    always @(posedge rd_clk or negedge rst) begin
        if (!rst)
            rd_clock_seen <= 1'b0;
        else
            rd_clock_seen <= 1'b1;
    end

    always @(posedge wr_clk or negedge rst) begin
        if (!rst)
            rd_clock_seen_sync <= 2'b00;
        else
            rd_clock_seen_sync <= {rd_clock_seen_sync[0], rd_clock_seen};
    end

    // Once both clocks are live, hold FIFO reset high for another 15 write
    // clocks so reset is observed safely in both domains.
    reg [3:0] rst_cnt    = 4'd0;
    reg       rst_fifo_n = 1'b0;

    always @(posedge wr_clk or negedge rst) begin
        if (!rst) begin
            rst_cnt    <= 4'd0;
            rst_fifo_n <= 1'b0;
        end
        else if (!rd_clock_seen_sync[1]) begin
            rst_cnt    <= 4'd0;
            rst_fifo_n <= 1'b0;
        end
        else if (rst_cnt < 4'd15) begin
            rst_cnt    <= rst_cnt + 1'b1;
            rst_fifo_n <= 1'b0;
        end
        else begin
            rst_fifo_n <= 1'b1;
        end
    end

    // Bit Width Conversion: Zero-pad lower 4 bits (12-bit to 16-bit)
    assign ad_data_in = {din, 4'b0000};

    // Xilinx FIFO Generator Core Instantiation
    fifo_generator_0 my_fifo_ip (
        .rst          (~rst_fifo_n),   // Active-high reset
        .wr_clk       (wr_clk),
        .rd_clk       (rd_clk),
        .din          (ad_data_in),
        .wr_en        (write_en),
        .rd_en        (read_en),
        .dout         (dout),
        .full         (fifo_full),
        .empty        (),
        .prog_full    (prog_full),
        .prog_empty   (prog_empty),
        .wr_rst_busy  (wr_rst_busy),
        .rd_rst_busy  (rd_rst_busy)
    );

endmodule
