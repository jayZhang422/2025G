`timescale 1ns / 1ps

module blk_rom_sine (
    input logic clka, input logic ena, input logic [11:0] addra,
    output logic [13:0] douta
);
    always_ff @(posedge clka) if (ena) douta <= 14'd8192;
endmodule

module blk_rom_triangle (
    input logic clka, input logic ena, input logic [11:0] addra,
    output logic [13:0] douta
);
    always_ff @(posedge clka) if (ena) douta <= 14'd8192;
endmodule

module tb_g2025_dac_adapter_v2;
    logic clk = 1'b0;
    logic rst_n = 1'b0;
    logic [31:0] control_bram [0:9];
    logic [31:0] ctrl_bram_addr, ctrl_bram_dout;
    logic ctrl_bram_en;
    logic [3:0] ctrl_bram_we;
    logic [31:0] wave_bram_addr;
    logic [15:0] wave_bram_dout;
    logic wave_bram_en;
    logic [1:0] wave_bram_we;
    logic [15:0] wave_bram [0:4095];
    logic [13:0] da_data_a, da_data_b;
    logic da_wrt_a, da_wrt_b;
    integer index, timeout;

    always #4 clk = ~clk;
    g2025_dac_adapter_v2 dut (
        .clk(clk), .rst_n(rst_n),
        .ctrl_bram_addr(ctrl_bram_addr), .ctrl_bram_dout(ctrl_bram_dout), .ctrl_bram_en(ctrl_bram_en), .ctrl_bram_we(ctrl_bram_we),
        .wave_bram_addr(wave_bram_addr), .wave_bram_dout(wave_bram_dout), .wave_bram_en(wave_bram_en), .wave_bram_we(wave_bram_we),
        .da_data_a(da_data_a), .da_data_b(da_data_b), .da_wrt_a(da_wrt_a), .da_wrt_b(da_wrt_b)
    );
    always_ff @(posedge clk) begin
        if (ctrl_bram_en) ctrl_bram_dout <= control_bram[ctrl_bram_addr[5:2]];
        if (wave_bram_en) wave_bram_dout <= wave_bram[wave_bram_addr[12:1]];
    end
    task automatic wait_for_commit(input logic [31:0] expected);
        begin : wait_commit
            for (timeout = 0; timeout < 80; timeout = timeout + 1) begin
                @(posedge clk); #1;
                if (dut.u_dac_dds.last_commit_seq === expected) disable wait_commit;
            end
            $fatal(1, "Timed out waiting for commit %0d", expected);
        end
    endtask
    initial begin
        for (index = 0; index < 10; index = index + 1) control_bram[index] = 32'd0;
        for (index = 0; index < 4096; index = index + 1) wave_bram[index] = 16'd8192;
        wave_bram[0] = 16'd12288;
        wave_bram[1] = 16'd4096;
        repeat (5) @(posedge clk);
        rst_n = 1'b1;
        repeat (4) @(negedge clk);
        if (da_data_a !== 14'd8192 || da_data_b !== 14'd8192) $fatal(1, "reset midscale failure");
        control_bram[0] = 32'd2;
        control_bram[1] = 32'h00100000;
        control_bram[3] = 32'd16383;
        control_bram[4] = 32'd0;
        control_bram[7] = 32'd0;
        control_bram[8] = 32'd3;
        control_bram[9] = 32'd1;
        wait_for_commit(32'd1);
        repeat (5) @(posedge clk);
        #1;
        if (wave_bram_we !== 2'b00 || wave_bram_addr[31:13] !== 19'd0) $fatal(1, "wave RAM must be read-only and byte addressed");
        if (wave_bram_addr[12:1] == 12'd0) $fatal(1, "DDS address did not advance by a 16-bit sample");
        if (da_data_b !== 14'd8192) $fatal(1, "DAC B must remain midscale at zero amplitude");
        control_bram[8] = 32'd0;
        control_bram[9] = 32'd2;
        wait_for_commit(32'd2);
        repeat (2) @(negedge clk);
        if (da_data_a !== 14'd8192 || da_data_b !== 14'd8192) $fatal(1, "atomic stop failure");
        $display("G2025_DAC_ADAPTER_V2_REGRESSION_PASSED");
        $finish;
    end
endmodule
