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
    input  wire [11:0] din,          // 12-bit data from AD9226
    input  wire        clk_phase,    // input phase clk (7ns)   
    output reg  [11:0] dout,         // registered sample output
    output wire        ad_out_valid  // high every cycle when data is valid
);


reg [6:0] delay_cnt;
reg       stable;

always @(posedge clk_phase or negedge rst_ad) begin
    if (!rst_ad ) begin
        delay_cnt <= 7'd0;
        stable    <= 1'b0;
        dout      <= 12'd0;
    end
    else if (!stable) begin
        if (delay_cnt < 7'd64)
            delay_cnt <= delay_cnt + 1'b1;
        else begin
            // Present a real registered ADC sample before ad_out_valid can
            // enable the first FIFO write on the following clock edge.
            dout   <= din;
            stable <= 1'b1;
        end
    end
    else begin
        dout <= din;
    end
end

assign ad_out_valid = stable ;

endmodule
