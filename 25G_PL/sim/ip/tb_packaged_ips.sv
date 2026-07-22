`timescale 1ns / 1ps

module tb_packaged_ips;
    logic clk_dac = 1'b0;
    logic clk_phase = 1'b0;
    logic rd_clk = 1'b0;
    logic rst_n = 1'b0;

    always #4  clk_dac   = ~clk_dac;
    always #10 clk_phase = ~clk_phase;
    always #5  rd_clk    = ~rd_clk;

    logic [31:0] control_bram [0:9];
    logic [31:0] bram_addr;
    logic [31:0] bram_dout = 32'd0;
    logic bram_en;
    logic [3:0] bram_we;
    logic [11:0] sine_addr_a, sine_addr_b;
    logic [11:0] triangle_addr_a, triangle_addr_b;
    logic [11:0] arb_addr_a, arb_addr_b;
    logic [13:0] sine_data_a = 14'd8192;
    logic [13:0] sine_data_b = 14'd8192;
    logic [13:0] triangle_data_a = 14'd8192;
    logic [13:0] triangle_data_b = 14'd8192;
    logic [13:0] arb_data_a = 14'd12288;
    logic [13:0] arb_data_b = 14'd4096;
    logic [13:0] da_data_a, da_data_b;
    logic da_wrt_a, da_wrt_b;

    DAC_DDS_Output u_dds (
        .clk(clk_dac), .rst_n(rst_n),
        .bram_addr(bram_addr), .bram_dout(bram_dout),
        .bram_en(bram_en), .bram_we(bram_we),
        .sine_addr_a(sine_addr_a), .sine_data_a(sine_data_a),
        .sine_addr_b(sine_addr_b), .sine_data_b(sine_data_b),
        .triangle_addr_a(triangle_addr_a), .triangle_data_a(triangle_data_a),
        .triangle_addr_b(triangle_addr_b), .triangle_data_b(triangle_data_b),
        .arb_addr_a(arb_addr_a), .arb_data_a(arb_data_a),
        .arb_addr_b(arb_addr_b), .arb_data_b(arb_data_b),
        .da_data_a(da_data_a), .da_data_b(da_data_b),
        .da_wrt_a(da_wrt_a), .da_wrt_b(da_wrt_b)
    );

    always @(posedge clk_dac) begin
        if (bram_en)
            bram_dout <= control_bram[bram_addr[5:2]];
        sine_data_a <= 14'd8192;
        sine_data_b <= 14'd8192;
        triangle_data_a <= 14'd8192;
        triangle_data_b <= 14'd8192;
        arb_data_a <= 14'd12288;
        arb_data_b <= 14'd4096;
    end

    logic [11:0] adc_din = 12'd0;
    logic rd_en = 1'b0;
    logic [15:0] adc_dout;
    logic adc_empty;

    always @(negedge clk_phase) begin
        if (!rst_n)
            adc_din <= 12'd0;
        else
            adc_din <= adc_din + 1'b1;
    end

    ad_fifo_warpper u_adc_fifo (
        .rst_n(rst_n), .clk_phase(clk_phase), .adc_din(adc_din),
        .rd_clk(rd_clk), .rd_en(rd_en), .dout(adc_dout), .empty(adc_empty)
    );

    task automatic wait_for_commit(input logic [31:0] expected);
        integer timeout;
        begin : commit_wait
            for (timeout = 0; timeout < 50; timeout = timeout + 1) begin
                @(posedge clk_dac);
                #1;
                if (u_dds.last_commit_seq === expected)
                    disable commit_wait;
            end
            $fatal(1, "Timeout waiting for DDS commit %0d", expected);
        end
    endtask

    integer i, timeout, changed_samples;
    logic [15:0] previous_sample;
    logic [31:0] previous_phase;

    initial begin
        for (i = 0; i < 10; i = i + 1)
            control_bram[i] = 32'd0;

        repeat (5) @(posedge clk_dac);
        rst_n = 1'b1;
        repeat (3) @(posedge clk_dac);
        #1;
        if (da_data_a !== 14'd8192 || da_data_b !== 14'd8192)
            $fatal(1, "DDS reset output is not midscale");

        control_bram[0] = 32'd2;
        control_bram[1] = 32'h10000000;
        control_bram[2] = 32'd0;
        control_bram[3] = 32'd16383;
        control_bram[4] = 32'd2;
        control_bram[5] = 32'h08000000;
        control_bram[6] = 32'h40000000;
        control_bram[7] = 32'd16383;
        control_bram[8] = 32'd3;

        repeat (20) @(posedge clk_dac);
        #1;
        if (u_dds.run_enabled !== 1'b0 || da_data_a !== 14'd8192)
            $fatal(1, "DDS shadow fields leaked before COMMIT_SEQ");

        control_bram[9] = 32'd1;
        wait_for_commit(32'd1);
        if (u_dds.run_wave_a !== 2'd2 || u_dds.run_wave_b !== 2'd2)
            $fatal(1, "Arbitrary waveform mode was not committed");
        if (u_dds.phase_acc_a !== 32'd0 || u_dds.phase_acc_b !== 32'h40000000)
            $fatal(1, "DDS phase reload was not atomic");

        repeat (4) @(negedge clk_dac);
        #1;
        if (da_data_a !== 14'd12287 || da_data_b !== 14'd4096)
            $fatal(1, "Arbitrary scaling failed: A=%0d B=%0d", da_data_a, da_data_b);
        if (da_wrt_a !== 1'b0 || da_wrt_b !== 1'b0)
            $fatal(1, "DAC data update is not on the falling clock phase");

        previous_phase = u_dds.phase_acc_a;
        @(posedge clk_dac);
        #1;
        if (u_dds.phase_acc_a !== previous_phase + 32'h10000000)
            $fatal(1, "DDS accumulator frequency step failed");

        control_bram[8] = 32'd0;
        control_bram[9] = 32'd2;
        wait_for_commit(32'd2);
        repeat (2) @(negedge clk_dac);
        #1;
        if (u_dds.run_enabled !== 1'b0 ||
            da_data_a !== 14'd8192 || da_data_b !== 14'd8192)
            $fatal(1, "Atomic stop did not force DAC midscale");

        begin : fifo_wait
            for (timeout = 0; timeout < 250; timeout = timeout + 1) begin
                @(posedge clk_phase);
                if (adc_empty === 1'b0)
                    disable fifo_wait;
            end
            $fatal(1, "ADC FIFO prog_empty did not deassert at its threshold");
        end

        rd_en = 1'b1;
        changed_samples = 0;
        previous_sample = adc_dout;
        repeat (12) begin
            @(posedge rd_clk);
            #1;
            if (adc_dout[3:0] !== 4'b0000)
                $fatal(1, "ADC FIFO output is not high-aligned: %h", adc_dout);
            if (adc_dout !== previous_sample)
                changed_samples = changed_samples + 1;
            previous_sample = adc_dout;
        end
        rd_en = 1'b0;
        if (changed_samples < 8)
            $fatal(1, "ADC FIFO did not deliver a changing sample stream");

        $display("PACKAGED_IP_REGRESSION_PASSED");
        $finish;
    end
endmodule
