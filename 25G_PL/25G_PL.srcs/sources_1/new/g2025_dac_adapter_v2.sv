`timescale 1ns / 1ps

// Read-only DDS client for the dedicated 4096 x 14 arbitrary-wave table.
module g2025_dac_adapter_v2 (
    input  logic        clk,
    input  logic        rst_n,
    output logic [31:0] ctrl_bram_addr,
    input  logic [31:0] ctrl_bram_dout,
    output logic        ctrl_bram_en,
    output logic [3:0]  ctrl_bram_we,
    output logic [31:0] wave_bram_addr,
    input  logic [15:0] wave_bram_dout,
    output logic        wave_bram_en,
    output logic [1:0]  wave_bram_we,
    output logic [13:0] da_data_a,
    output logic [13:0] da_data_b,
    output logic        da_wrt_a,
    output logic        da_wrt_b
);
    logic [11:0] sine_addr_a, sine_addr_b;
    logic [11:0] triangle_addr_a, triangle_addr_b;
    logic [11:0] arb_addr_a, arb_addr_b;
    logic [13:0] sine_data_a, sine_data_b;
    logic [13:0] triangle_data_a, triangle_data_b;

    // Native BRAM addresses are byte addressed.  One table sample is 16 bits.
    assign wave_bram_addr = {19'd0, arb_addr_a, 1'b0};
    assign wave_bram_en   = 1'b1;
    assign wave_bram_we   = 2'b00;

    blk_rom_sine u_sine_a (.clka(clk), .ena(1'b1), .addra(sine_addr_a), .douta(sine_data_a));
    blk_rom_sine u_sine_b (.clka(clk), .ena(1'b1), .addra(sine_addr_b), .douta(sine_data_b));
    blk_rom_triangle u_triangle_a (.clka(clk), .ena(1'b1), .addra(triangle_addr_a), .douta(triangle_data_a));
    blk_rom_triangle u_triangle_b (.clka(clk), .ena(1'b1), .addra(triangle_addr_b), .douta(triangle_data_b));

    DAC_DDS_Output u_dac_dds (
        .clk(clk), .rst_n(rst_n),
        .bram_addr(ctrl_bram_addr), .bram_dout(ctrl_bram_dout),
        .bram_en(ctrl_bram_en), .bram_we(ctrl_bram_we),
        .sine_addr_a(sine_addr_a), .sine_data_a(sine_data_a),
        .sine_addr_b(sine_addr_b), .sine_data_b(sine_data_b),
        .triangle_addr_a(triangle_addr_a), .triangle_data_a(triangle_data_a),
        .triangle_addr_b(triangle_addr_b), .triangle_data_b(triangle_data_b),
        .arb_addr_a(arb_addr_a), .arb_data_a(wave_bram_dout[13:0]),
        .arb_addr_b(arb_addr_b), .arb_data_b(wave_bram_dout[13:0]),
        .da_data_a(da_data_a), .da_data_b(da_data_b),
        .da_wrt_a(da_wrt_a), .da_wrt_b(da_wrt_b)
    );
endmodule
