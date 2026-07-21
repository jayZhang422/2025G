`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Module Name : ad9226
// Revision    : 2026.6.28
// Description : AD9226 sampling wrapper
//               fs = 5.12MHz, N = 4096, df = 1250Hz
//               clk_ad    : 5.12MHz 0deg   -> AD9226 CLK pin
//               clk_ad_deg: 5.12MHz 181.8deg -> samples din at center of valid window
//////////////////////////////////////////////////////////////////////////////////
module ad9226 (
    input  wire        rst_ad,       // active-low reset
    input  wire        clk,          // 50MHz system clock
    input  wire [11:0] din,          // 12-bit data from AD9226

    output reg  [11:0] dout,         // registered sample output
    output wire        clk_sys_out,  // 65MHz -> for other modules
    output wire        clk_ad,       // 5.12MHz 0deg   -> AD9226 CLK pin
    output wire        clk_ad_deg,   // 5.12MHz 181.8deg -> exported for reference
    output wire        ad_out_valid  // high every cycle when data is valid
);

wire clk_pll_out;  // 65MHz
wire clk_pll_ad;   // 5.12MHz 0deg
wire clk_pll_deg;  // 5.12MHz 181.8deg
wire locked;

PLL_AD ad_clk (
    .clk_pll_out (clk_pll_out),
    .clk_pll_ad  (clk_pll_ad),
    .clk_pll_deg (clk_pll_deg),
    .resetn      (rst_ad), 
    .locked      (locked),
    .clk_sys     (clk)
);

assign clk_sys_out = clk_pll_out;
assign clk_ad      = clk_pll_ad;
assign clk_ad_deg  = clk_pll_deg;

reg [6:0] delay_cnt;
reg       stable;

always @(posedge clk_pll_deg or negedge rst_ad) begin
    if (!rst_ad || !locked) begin
        delay_cnt <= 7'd0;
        stable    <= 1'b0;
        dout      <= 12'd0;
    end
    else if (!stable) begin
        if (delay_cnt < 7'd64)
            delay_cnt <= delay_cnt + 1'b1;
        else
            stable <= 1'b1;
    end
    else begin
        dout <= din;
    end
end

assign ad_out_valid = stable & locked;

endmodule