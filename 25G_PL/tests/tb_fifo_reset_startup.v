`timescale 1ns / 1ps

// Standalone regression; compile with fifo.v and the local FIFO stub below.
module tb_fifo_reset_startup;
    reg         rst;
    reg         wr_clk;
    reg         rd_clk;
    reg         rd_clock_enable;
    wire [15:0] dout;
    wire        empty;

    fifo dut (
        .rst            (rst),
        .wr_clk         (wr_clk),
        .wr_en          (1'b1),
        .din            (12'h123),
        .rd_clk         (rd_clk),
        .rd_en          (1'b0),
        .dout           (dout),
        .empty          (empty),
        .mon_write_en   (),
        .mon_prog_full  (),
        .mon_fifo_full  (),
        .mon_wr_rst_busy(),
        .mon_rd_rst_busy()
    );

    initial begin
        wr_clk = 1'b0;
        forever #50 wr_clk = ~wr_clk;
    end

    initial begin
        rd_clk = 1'b0;
        forever begin
            #5;
            if (rd_clock_enable)
                rd_clk = ~rd_clk;
        end
    end

    initial begin
        rst = 1'b0;
        rd_clock_enable = 1'b0;
        #200;
        rst = 1'b1;

        repeat (30) @(posedge wr_clk);
        #1;
        if (dut.rst_fifo_n !== 1'b0)
            $fatal(1, "FIFO reset released before FCLK read clock started");

        rd_clock_enable = 1'b1;
        repeat (20) @(posedge wr_clk);
        #1;
        if (dut.rst_fifo_n !== 1'b1)
            $fatal(1, "FIFO reset did not release after both clocks started");

        $display("PASS: FIFO reset waits for the read clock before release");
        $finish;
    end
endmodule

module fifo_generator_0 (
    input  wire        rst,
    input  wire        wr_clk,
    input  wire        rd_clk,
    input  wire [15:0] din,
    input  wire        wr_en,
    input  wire        rd_en,
    output wire [15:0] dout,
    output wire        full,
    output wire        empty,
    output wire        prog_full,
    output wire        prog_empty,
    output wire        wr_rst_busy,
    output wire        rd_rst_busy
);
    assign dout        = din;
    assign full        = 1'b0;
    assign empty       = 1'b1;
    assign prog_full   = 1'b0;
    assign prog_empty  = 1'b1;
    assign wr_rst_busy = rst;
    assign rd_rst_busy = rst;
endmodule
