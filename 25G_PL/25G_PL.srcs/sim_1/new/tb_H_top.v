`timescale 1ns / 1ps

module tb_H_top;
    // =========================================================================
    // Simulated PS configuration parameters
    // =========================================================================
    localparam [31:0] PS_WAVE_A      = 32'd0;       // Channel A: Sine Wave[cite: 1]
    localparam [31:0] PS_STEP_A      = 32'd8589934; // Channel A: 250 kHz @ 125MHz DAC Clock
    localparam [31:0] PS_PHASE_A     = 32'd0;       // Channel A: Initial Phase 0
    localparam [31:0] PS_AMP_A       = 32'd8191;    // Channel A: Full Scale
    
    localparam [31:0] PS_WAVE_B      = 32'd1;       // Channel B: Triangle Wave[cite: 1]
    localparam [31:0] PS_STEP_B      = 32'd4294967; // Channel B: 125 kHz
    localparam [31:0] PS_PHASE_B     = 32'd1073741824; // Channel B: Initial Phase 90 Degrees
    localparam [31:0] PS_AMP_B       = 32'd4095;    // Channel B: Half Scale

    localparam [31:0] TRACKING_STEP_A = 32'd8600000; // Simulated micro-tuning step for PI loop

    // Interface Signals
    reg         i_clk_50m;
    reg         i_clk_100m;
    reg         i_clk_dac;
    reg         i_rst;
    reg  [11:0] i_ad_data;
    reg         m_axis_tready;

    wire        o_ad_clk;
    wire [13:0] o_da_data;
    wire        o_da_wrt;
    wire        o_da_clk;
    wire [13:0] o_da_data_b;
    wire        o_da_wrt_b;
    wire        o_da_clk_b;
    wire [15:0] m_axis_tdata;
    wire        m_axis_tvalid;
    wire        m_axis_tlast;
    
    wire [31:0] bram_addr;
    wire        bram_en;
    wire [3:0]  bram_we;
    reg  [31:0] bram_dout;
    
    reg  [31:0] previous_phase_a;
    reg  [31:0] previous_phase_b;
    reg  [13:0] previous_dac_a;
    integer      phase_check_index;
    integer dac_step;
    integer i;
    integer phase_deg;
    reg [31:0] phase_word;
    reg        dac_timing_sample_valid;
    reg [13:0] dac_data_a_before_latch;
    reg [13:0] dac_data_b_before_latch;

    function [31:0] phase_from_degrees;
        input integer degrees;
        reg [63:0] numerator;
        begin
            numerator = degrees * 64'd4294967296;
            phase_from_degrees = numerator / 360;
        end
    endfunction

    // BRAM Emulation Array (Size 10 for dual channels & control block)[cite: 1]
    reg [31:0] ps_bram [0:9];

    // Instantiate Top Module
    H_top uut (
        .i_clk_50m(i_clk_50m), .i_clk_100m(i_clk_100m),
        .i_clk_dac(i_clk_dac), .i_rst(i_rst), .i_ad_data(i_ad_data),
        .o_ad_clk(o_ad_clk), .o_da_data(o_da_data), .o_da_wrt(o_da_wrt),
        .o_da_clk(o_da_clk), .o_da_data_b(o_da_data_b),
        .o_da_wrt_b(o_da_wrt_b), .o_da_clk_b(o_da_clk_b),
        .m_axis_tdata(m_axis_tdata), .m_axis_tvalid(m_axis_tvalid),
        .m_axis_tready(m_axis_tready), .m_axis_tlast(m_axis_tlast),
        .bram_addr(bram_addr), .bram_en(bram_en), .bram_we(bram_we),
        .bram_dout(bram_dout)
    );

    // Clock Generation
    initial begin
        i_clk_50m  = 1'b0; i_clk_100m = 1'b0; i_clk_dac  = 1'b0;
    end
    always #10 i_clk_50m  = ~i_clk_50m;  // 50 MHz[cite: 1]
    always #5  i_clk_100m = ~i_clk_100m; // 100 MHz[cite: 1]
    always #4  i_clk_dac  = ~i_clk_dac;  // 125 MHz[cite: 1]

    // Data must update on the falling edge and remain stable until the next
    // common WRT/CLK rising edge that latches it at the AD9767 pins.
    always @(negedge o_da_clk) begin
        #1;
        if (i_rst) begin
            if (o_da_clk !== 1'b0 || o_da_wrt !== 1'b0 ||
                o_da_clk_b !== 1'b0 || o_da_wrt_b !== 1'b0)
                $fatal(1, "ERROR: DAC WRT/CLK falling-edge phase mismatch.");
            dac_data_a_before_latch = o_da_data;
            dac_data_b_before_latch = o_da_data_b;
            dac_timing_sample_valid = 1'b1;
        end
    end

    always @(posedge o_da_clk) begin
        #1;
        if (i_rst && dac_timing_sample_valid) begin
            if (o_da_clk !== 1'b1 || o_da_wrt !== 1'b1 ||
                o_da_clk_b !== 1'b1 || o_da_wrt_b !== 1'b1)
                $fatal(1, "ERROR: DAC WRT/CLK rising-edge phase mismatch.");
            if (dac_timing_sample_valid &&
                (o_da_data !== dac_data_a_before_latch ||
                 o_da_data_b !== dac_data_b_before_latch))
                $fatal(1, "ERROR: DAC data changed at its WRT/CLK edge.");
        end
    end

    // BRAM read logic
    always @(posedge i_clk_dac) begin
        if (bram_en)
            bram_dout <= ps_bram[bram_addr[5:2]]; 
    end

    // ADC Dummy Data Generator
    always @(posedge i_clk_50m or negedge i_rst) begin
        if (!i_rst) i_ad_data <= 12'd0;
        else        i_ad_data <= i_ad_data + 1'b1;
    end

    // Tasks for PS emulation
    task ps_write;
        input [3:0]  word_index;
        input [31:0] data;
        begin
            ps_bram[word_index] = data;
        end
    endtask

    task wait_for_commit;
        input [31:0] expected_seq;
        integer timeout;
        begin : wait_loop
            for (timeout = 0; timeout < 40; timeout = timeout + 1) begin
                @(posedge i_clk_dac);
                #1;
                if (uut.u_dac_dds.inst.last_commit_seq === expected_seq)
                    disable wait_loop;
            end
            $fatal(1, "ERROR: Timeout waiting for COMMIT_SEQ=%0d", expected_seq);
        end
    endtask

    task wait_for_commit_with_phase;
        input [31:0] expected_seq;
        output [31:0] phase_before_a;
        output [31:0] phase_before_b;
        integer timeout;
        begin : wait_loop
            for (timeout = 0; timeout < 40; timeout = timeout + 1) begin
                @(negedge i_clk_dac);
                phase_before_a = uut.u_dac_dds.inst.phase_acc_a;
                phase_before_b = uut.u_dac_dds.inst.phase_acc_b;
                @(posedge i_clk_dac);
                #1;
                if (uut.u_dac_dds.inst.last_commit_seq === expected_seq)
                    disable wait_loop;
            end
            $fatal(1, "ERROR: Timeout waiting for COMMIT_SEQ=%0d with phase sampling", expected_seq);
        end
    endtask

    // =========================================================================
    // Main Stimulus
    // =========================================================================
    initial begin
        // 0. Initialization
        i_rst = 1'b0;
        i_ad_data = 12'd0;
        m_axis_tready = 1'b1;
        bram_dout = 32'd0;
        dac_timing_sample_valid = 1'b0;
        
        for (i = 0; i < 10; i = i + 1) begin
            ps_write(i, 32'd0);
        end

        #60 i_rst = 1'b1; // Release Reset
        repeat (2) @(posedge i_clk_dac);
        #1;
        if (uut.u_dac_dds.inst.run_enabled !== 1'b0 ||
            uut.u_dac_dds.inst.shadow_commit_seq !== 32'd0 ||
            o_da_data !== 14'd8192 || o_da_data_b !== 14'd8192)
            $fatal(1, "ERROR: DDS or DAC midscale state uncertain after reset.");

        $display("\n=======================================================");
        $display(" STAGE 1: Initial Start (Testing Atomic Commit & Forced Initial Phase)");
        $display("=======================================================");
        ps_write(4'd0, PS_WAVE_A);
        ps_write(4'd1, PS_STEP_A);
        ps_write(4'd2, PS_PHASE_A);
        ps_write(4'd3, PS_AMP_A);
        
        ps_write(4'd4, PS_WAVE_B);
        ps_write(4'd5, PS_STEP_B);
        ps_write(4'd6, PS_PHASE_B);
        ps_write(4'd7, PS_AMP_B);
        
        ps_write(4'd8, 32'd3); // RUN=1, RELOAD=1
        
        repeat (12) @(posedge i_clk_dac);
        #1;
        $display("  --> Testing shadow register isolation... (run_step_a should be 0)");
        if (uut.u_dac_dds.inst.run_enabled !== 1'b0 ||
            uut.u_dac_dds.inst.run_step_a !== 32'd0 ||
            o_da_data !== 14'd8192 || o_da_data_b !== 14'd8192)
            $fatal(1, "ERROR: Atomic commit not triggered, but running registers or DAC outputs changed!");

        $display("  --> PS updates COMMIT_SEQ to trigger atomic launch!");
        ps_write(4'd9, 32'd1); 
        
        wait_for_commit(32'd1);
        if (uut.u_dac_dds.inst.run_step_a !== PS_STEP_A) $fatal(1, "ERROR: Atomic commit failed, run_step_a not updated!");
        if (uut.u_dac_dds.inst.run_step_b !== PS_STEP_B ||
            uut.u_dac_dds.inst.run_wave_b !== PS_WAVE_B[0] ||
            uut.u_dac_dds.inst.run_amp_b !== PS_AMP_B[13:0])
            $fatal(1, "ERROR: Channel B parameters not atomically updated!");
        if (uut.u_dac_dds.inst.phase_acc_a !== PS_PHASE_A) $fatal(1, "ERROR: Initial start failed to force phase_acc_a!");
        if (uut.u_dac_dds.inst.phase_acc_b !== PS_PHASE_B) $fatal(1, "ERROR: Initial start failed to force phase_acc_b!");
        $display("  --> STAGE 1 PASSED! Dual-channel parameters synchronized instantly, initial phases forced perfectly.");


        $display("\n=======================================================");
        $display(" STAGE 2: Frequency Tracking Verification (Waveform Continuity Check)");
        $display("=======================================================");
        #1;
        previous_phase_a = uut.u_dac_dds.inst.phase_acc_a;
        previous_phase_b = uut.u_dac_dds.inst.phase_acc_b;
        previous_dac_a   = o_da_data;
        dac_step = 0;
        for (phase_check_index = 0; phase_check_index < 32; phase_check_index = phase_check_index + 1) begin
            @(posedge i_clk_dac);
            #1;
            if (uut.u_dac_dds.inst.phase_acc_a !== previous_phase_a + uut.u_dac_dds.inst.run_step_a) begin
                $fatal(1, "ERROR: DDS accumulator stalled during normal run! Sample: %0d", phase_check_index);
            end
            if (uut.u_dac_dds.inst.phase_acc_b !== previous_phase_b + uut.u_dac_dds.inst.run_step_b)
                $fatal(1, "ERROR: Channel B accumulator stalled during normal run! Sample: %0d", phase_check_index);
            if (o_da_data !== o_da_data_b)
                dac_step = 1;
            previous_phase_a = uut.u_dac_dds.inst.phase_acc_a;
            previous_phase_b = uut.u_dac_dds.inst.phase_acc_b;
            previous_dac_a = o_da_data;
        end
        if (dac_step == 0)
            $fatal(1, "ERROR: A/B DAC outputs are identical, independent channels failed!");


        $display("\n=======================================================");
        $display(" STAGE 3: PI Tracking Micro-adjustment (Modify Step Only, Keep Phase Continuous)");
        $display("=======================================================");
        ps_write(4'd8, 32'd0);
        repeat (12) @(posedge i_clk_dac);
        #1;
        if (uut.u_dac_dds.inst.run_enabled !== 1'b1)
            $fatal(1, "ERROR: Uncommitted RUN shadow value stopped DDS prematurely!");

        ps_write(4'd1, TRACKING_STEP_A); 
        ps_write(4'd8, 32'd1); // RUN=1, RELOAD=0
        
        $display("  --> PS updates COMMIT_SEQ (PI closed-loop tuning effective)...");
        ps_write(4'd9, 32'd2); 

        wait_for_commit_with_phase(32'd2, previous_phase_a, previous_phase_b);

        if (uut.u_dac_dds.inst.run_step_a !== TRACKING_STEP_A) $fatal(1, "ERROR: Frequency tracking step update failed!");
        if (uut.u_dac_dds.inst.phase_acc_a !== previous_phase_a + TRACKING_STEP_A ||
            uut.u_dac_dds.inst.phase_acc_b !== previous_phase_b + PS_STEP_B)
            $fatal(1, "FATAL ERROR: Tracking commit did not maintain phase continuity for both channels!");
        
        $display("  --> STAGE 3 PASSED! Parameter updated successfully without interrupting accumulators.");


        $display("\n=======================================================");
        $display(" STAGE 4: Sine/Sine 0 to 180 Degree, 5-Degree Step Initial Phase Verification");
        $display("=======================================================");
        ps_write(4'd0, 32'd0);
        ps_write(4'd1, PS_STEP_B);
        ps_write(4'd2, 32'd0);
        ps_write(4'd3, PS_AMP_A);
        ps_write(4'd4, 32'd0);
        ps_write(4'd5, PS_STEP_A);
        ps_write(4'd7, PS_AMP_A);
        ps_write(4'd8, 32'd3);

        for (phase_deg = 0; phase_deg <= 180; phase_deg = phase_deg + 5) begin
            phase_word = phase_from_degrees(phase_deg);
            ps_write(4'd6, phase_word);
            ps_write(4'd9, 32'd3 + (phase_deg / 5));
            wait_for_commit(32'd3 + (phase_deg / 5));
            if (uut.u_dac_dds.inst.run_wave_a !== 1'b0 ||
                uut.u_dac_dds.inst.run_wave_b !== 1'b0 ||
                uut.u_dac_dds.inst.phase_acc_a !== 32'd0 ||
                uut.u_dac_dds.inst.phase_acc_b !== phase_word)
                $fatal(1, "ERROR: Initial phase commit error at %0d degrees", phase_deg);
        end
        $display("  --> STAGE 4 PASSED! 0 to 180 degree, 5-degree steps loaded on common commit edge.");


        $display("\n=======================================================");
        $display(" STAGE 5: Live B Phase Adjustment (Keep A Continuous)");
        $display("=======================================================");
        phase_word = phase_from_degrees(5);
        ps_write(4'd6, phase_word);
        ps_write(4'd8, 32'd5); // RUN=1, B phase delta=1
        ps_write(4'd9, 32'd40);
        wait_for_commit_with_phase(32'd40, previous_phase_a, previous_phase_b);
        if (uut.u_dac_dds.inst.phase_acc_a !== previous_phase_a + PS_STEP_B ||
            uut.u_dac_dds.inst.phase_acc_b !== previous_phase_b + PS_STEP_A + phase_word)
            $fatal(1, "ERROR: Live B phase adjustment changed A or missed B delta!");
        $display("  --> STAGE 5 PASSED! A stayed continuous and B advanced by 5 degrees.");


        $display("\n=======================================================");
        $display(" STAGE 6: RUN=0 Atomic Stop & DAC Midscale Verification");
        $display("=======================================================");
        ps_write(4'd8, 32'd0);
        ps_write(4'd9, 32'd41);
        wait_for_commit(32'd41);
        repeat (3) @(posedge i_clk_dac);
        #1;
        if (uut.u_dac_dds.inst.run_enabled !== 1'b0 ||
            uut.u_dac_dds.inst.phase_acc_a !== 32'd0 ||
            uut.u_dac_dds.inst.phase_acc_b !== 32'd0 ||
            o_da_data !== 14'd8192 || o_da_data_b !== 14'd8192)
            $fatal(1, "ERROR: DDS or DAC midscale state incorrect after RUN=0 commit!");
        $display("  --> STAGE 5 PASSED! Stop commit acknowledged, both DACs maintained midscale.");


        // =========================================================================
        // STAGE 7: AXIS End-of-Frame TLAST Gating & Backpressure Verification[cite: 1]
        // =========================================================================
        $display("\n=======================================================");
        $display(" STAGE 7: AXIS End-of-Frame TLAST Gating & Backpressure Verification");
        $display("=======================================================");
        
        $display("  --> Waiting for ADC and FIFO stabilization (Waiting for m_axis_tvalid = 1)...");
        wait (m_axis_tvalid == 1'b1);
        
        @(posedge i_clk_100m);
        force uut.w_tlast_cnt = 12'd4094; // Backdoor seed[cite: 1]
        #1;
        release uut.w_tlast_cnt;
        $display("  --> [Backdoor Fast-forward] Forced internal w_tlast_cnt to: %0d", uut.w_tlast_cnt);
        
        @(posedge i_clk_100m);
        while (!(m_axis_tvalid && m_axis_tready)) begin
            @(posedge i_clk_100m); 
        end
        #1; 
        $display("  --> [Beat 4095 Delivered] Current counter w_tlast_cnt incremented to: %0d", uut.w_tlast_cnt);
        
        wait (m_axis_tvalid == 1'b1);
        m_axis_tready = 1'b0; // Block receiver[cite: 1]
        #1; 
        
        $display("  --> [Interception Test] Snapshot: w_tlast_cnt=%0d, tvalid=%b, tready=%b", uut.w_tlast_cnt, m_axis_tvalid, m_axis_tready);
        $display("  --> [Interception Test] Current output m_axis_tlast = %b", m_axis_tlast);
        
        if (m_axis_tlast !== 1'b1) begin
            $fatal(1, "FATAL ERROR: Fix failed! When TVALID=1 but TREADY=0, TLAST collapsed![cite: 1]");
        end
        $display("  --> SUCCESS! TLAST perfectly withstood backpressure without collapsing with TREADY![cite: 1]");
        
        repeat (3) @(posedge i_clk_100m);
        #1;
        if (uut.w_tlast_cnt !== 12'd4095) begin
            $fatal(1, "FATAL ERROR: During backpressure, counter incremented without TREADY! Handshake broken![cite: 1]");
        end
        $display("  --> SUCCESS! Counter remained stationary during backpressure, compliant with protocol.[cite: 1]");
        
        $display("  --> Removing backpressure, releasing m_axis_tready = 1");
        m_axis_tready = 1'b1;
        
        @(posedge i_clk_100m);
        #1;
        $display("  --> [Transfer Complete] Status Check: w_tlast_cnt=%0d, m_axis_tlast=%b", uut.w_tlast_cnt, m_axis_tlast);
        if (uut.w_tlast_cnt !== 12'd0 || m_axis_tlast !== 1'b0) begin
            $fatal(1, "FATAL ERROR: After final beat transfer, frame counter did not reset or TLAST failed to deassert!");
        end
        
        $display("\n=======================================================");
        $display(" >>> ALL TIMING TESTS PASSED PERFECTLY! DEFECT CONQUERED! <<<");
        $display("=======================================================\n");

        #500 $finish;
    end
endmodule
