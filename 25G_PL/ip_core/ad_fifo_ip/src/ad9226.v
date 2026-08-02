`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Module Name : ad9226 (legacy interface name retained)
// Revision    : 2026.8.2
// Description : AD9248 single-channel sampling register
//               fs = 65MHz, N = 4096, df = 15.87kHz
//               clk_ad    : 65MHz 0deg -> AD9248 CLK pins
//               clk_phase : phase-selected clock -> captures one mux channel
//////////////////////////////////////////////////////////////////////////////////
module ad9226 (
    input  wire        rst_ad,       // active-low reset
    input  wire [13:0] din,          // 14-bit multiplexed data from AD9248
    input  wire        clk_phase,    // input phase clk (7ns)   
    output reg  [13:0] dout,         // registered selected-channel sample
    output wire        ad_out_valid  // high every cycle when data is valid
);


reg [6:0] delay_cnt;
reg       stable;

always @(posedge clk_phase or negedge rst_ad) begin
    if (!rst_ad ) begin
        delay_cnt <= 7'd0;
        stable    <= 1'b0;
        dout      <= 14'd0;
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
