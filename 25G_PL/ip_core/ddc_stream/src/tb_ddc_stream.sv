`timescale 1ns / 1ps

module tb_ddc_stream;

    localparam real ADC_PERIOD_NS = 1.0e9 / 5_120_060.0;
    localparam real AXI_PERIOD_NS = 10.0;
    localparam real TWO_PI = 6.28318530717958647692;
    localparam real CW_PHASE_STEP = TWO_PI * 2_000_000.0 / 5_120_060.0;
    localparam logic [31:0] TEST_PINC = 32'h63ff_b334;

    logic clk_adc = 1'b0;
    logic rst_n = 1'b0;
    logic [11:0] i_adc_raw = 12'd2048;
    logic i_sample_valid = 1'b0;

    logic s_axi_aclk = 1'b0;
    logic s_axi_aresetn = 1'b0;
    logic [4:0] s_axi_awaddr = 5'd0;
    logic s_axi_awvalid = 1'b0;
    logic s_axi_awready;
    logic [31:0] s_axi_wdata = 32'd0;
    logic [3:0] s_axi_wstrb = 4'd0;
    logic s_axi_wvalid = 1'b0;
    logic s_axi_wready;
    logic [1:0] s_axi_bresp;
    logic s_axi_bvalid;
    logic s_axi_bready = 1'b1;
    logic [4:0] s_axi_araddr = 5'd0;
    logic s_axi_arvalid = 1'b0;
    logic s_axi_arready;
    logic [31:0] s_axi_rdata;
    logic [1:0] s_axi_rresp;
    logic s_axi_rvalid;
    logic s_axi_rready = 1'b1;

    logic [15:0] m_axis_tdata;
    logic m_axis_tvalid;
    logic m_axis_tready = 1'b0;
    logic m_axis_tlast;

    logic generate_cw = 1'b0;
    logic random_ready = 1'b0;
    logic force_stall = 1'b0;
    real adc_phase = 0.0;
    integer adc_code;

    integer beat_count = 0;
    integer complex_count = 0;
    integer signed i_sample_hold = 0;
    integer signed q_sample_now;
    longint signed sum_i = 0;
    longint signed sum_q = 0;
    longint signed sum_i_squared = 0;
    longint signed sum_q_squared = 0;
    longint signed dc_power_scaled;
    longint signed total_power_scaled;
    logic frame_done = 1'b0;
    logic [31:0] read_value;

    always #(ADC_PERIOD_NS / 2.0) clk_adc = ~clk_adc;
    always #(AXI_PERIOD_NS / 2.0) s_axi_aclk = ~s_axi_aclk;

    ddc_stream dut (
        .clk_adc(clk_adc),
        .rst_n(rst_n),
        .i_adc_raw(i_adc_raw),
        .i_sample_valid(i_sample_valid),
        .s_axi_aclk(s_axi_aclk),
        .s_axi_aresetn(s_axi_aresetn),
        .s_axi_awaddr(s_axi_awaddr),
        .s_axi_awvalid(s_axi_awvalid),
        .s_axi_awready(s_axi_awready),
        .s_axi_wdata(s_axi_wdata),
        .s_axi_wstrb(s_axi_wstrb),
        .s_axi_wvalid(s_axi_wvalid),
        .s_axi_wready(s_axi_wready),
        .s_axi_bresp(s_axi_bresp),
        .s_axi_bvalid(s_axi_bvalid),
        .s_axi_bready(s_axi_bready),
        .s_axi_araddr(s_axi_araddr),
        .s_axi_arvalid(s_axi_arvalid),
        .s_axi_arready(s_axi_arready),
        .s_axi_rdata(s_axi_rdata),
        .s_axi_rresp(s_axi_rresp),
        .s_axi_rvalid(s_axi_rvalid),
        .s_axi_rready(s_axi_rready),
        .m_axis_tdata(m_axis_tdata),
        .m_axis_tvalid(m_axis_tvalid),
        .m_axis_tready(m_axis_tready),
        .m_axis_tlast(m_axis_tlast)
    );

    task automatic axi_write(
        input logic [4:0] address,
        input logic [31:0] data
    );
        begin
            @(posedge s_axi_aclk);
            s_axi_awaddr <= address;
            s_axi_awvalid <= 1'b1;
            do @(posedge s_axi_aclk); while (!s_axi_awready);
            s_axi_awvalid <= 1'b0;

            s_axi_wdata <= data;
            s_axi_wstrb <= 4'hf;
            s_axi_wvalid <= 1'b1;
            do @(posedge s_axi_aclk); while (!s_axi_wready);
            s_axi_wvalid <= 1'b0;

            do @(posedge s_axi_aclk); while (!s_axi_bvalid);
            if (s_axi_bresp != 2'b00)
                $fatal(1, "AXI write 0x%02h returned BRESP=%b",
                       address, s_axi_bresp);
        end
    endtask

    task automatic axi_read(
        input  logic [4:0] address,
        output logic [31:0] data
    );
        begin
            @(posedge s_axi_aclk);
            s_axi_araddr <= address;
            s_axi_arvalid <= 1'b1;
            do @(posedge s_axi_aclk); while (!s_axi_arready);
            s_axi_arvalid <= 1'b0;

            do @(posedge s_axi_aclk); while (!s_axi_rvalid);
            if (s_axi_rresp != 2'b00)
                $fatal(1, "AXI read 0x%02h returned RRESP=%b",
                       address, s_axi_rresp);
            data = s_axi_rdata;
        end
    endtask

    always @(posedge clk_adc) begin
        if (!rst_n) begin
            adc_phase <= 0.0;
            i_adc_raw <= 12'd2048;
        end else if (generate_cw) begin
            adc_code = $rtoi(2048.0 + 900.0 * $cos(adc_phase));
            if (adc_code < 0)
                adc_code = 0;
            else if (adc_code > 4095)
                adc_code = 4095;
            i_adc_raw <= adc_code[11:0];
            adc_phase <= (adc_phase + CW_PHASE_STEP >= TWO_PI) ?
                         adc_phase + CW_PHASE_STEP - TWO_PI :
                         adc_phase + CW_PHASE_STEP;
        end
    end

    // Short random TREADY gaps must be absorbed by the asynchronous FIFO.
    always @(posedge s_axi_aclk) begin
        if (!s_axi_aresetn)
            m_axis_tready <= 1'b0;
        else if (force_stall)
            m_axis_tready <= 1'b0;
        else if (random_ready)
            m_axis_tready <= ($urandom_range(0, 7) != 0);
        else
            m_axis_tready <= 1'b1;
    end

    always @(posedge s_axi_aclk) begin
        if (m_axis_tvalid && m_axis_tready) begin
            if (m_axis_tlast && beat_count != 8191)
                $fatal(1, "TLAST arrived at beat %0d, expected 8191",
                       beat_count);
            if (!m_axis_tlast && beat_count == 8191)
                $fatal(1, "TLAST missing on Q[4095]");

            if ((beat_count & 1) == 0) begin
                i_sample_hold = $signed(m_axis_tdata);
            end else begin
                q_sample_now = $signed(m_axis_tdata);
                sum_i = sum_i + i_sample_hold;
                sum_q = sum_q + q_sample_now;
                sum_i_squared = sum_i_squared +
                                longint'(i_sample_hold * i_sample_hold);
                sum_q_squared = sum_q_squared +
                                longint'(q_sample_now * q_sample_now);
                complex_count = complex_count + 1;
            end

            beat_count = beat_count + 1;
            if (m_axis_tlast)
                frame_done <= 1'b1;
        end
    end

    initial begin
        // Independent watchdog: a correct first frame is about 25.6 ms.
        #70_000_000;
        $fatal(1, "testbench watchdog expired");
    end

    initial begin
        repeat (8) @(posedge s_axi_aclk);
        s_axi_aresetn <= 1'b1;
        repeat (8) @(posedge clk_adc);
        rst_n <= 1'b1;

        // Explicitly lock the board polarity requested for this implementation.
        i_adc_raw <= 12'd2500;
        repeat (2) @(posedge clk_adc);
        if ($signed(dut.u_core.adc_centered) != -13'sd452)
            $fatal(1, "ADC polarity is wrong: expected 2048-2500=-452, got %0d",
                   $signed(dut.u_core.adc_centered));

        // PINC is a shadow value and must not change the ADC-domain value until
        // a CTRL commit with RESTART.
        axi_write(5'h04, TEST_PINC);
        repeat (12) @(posedge clk_adc);
        if (dut.active_pinc_adc != 32'h63ff_b333)
            $fatal(1, "PINC changed before RESTART");

        i_sample_valid <= 1'b1;
        generate_cw <= 1'b1;
        axi_write(5'h00, 32'h0000_0007); // RUN | RESTART | CLEAR_FAULT
        wait (!dut.cfg_busy);
        repeat (4) @(posedge clk_adc);
        if (dut.active_pinc_adc != TEST_PINC)
            $fatal(1, "PINC was not applied at RESTART");

        fork
            begin
                wait (dut.running_sync2);
            end
            begin
                #100_000;
                $fatal(1, "DDC did not leave FIFO reset state");
            end
        join_any
        disable fork;

        repeat (64) @(posedge clk_adc);
        if (dut.core_fault)
            $fatal(1, "FIR input TREADY dropped: set FIR Clock Frequency to 5.12006 MHz when FIR aclk=clk_adc");

        random_ready <= 1'b1;
        fork
            begin
                wait (frame_done);
            end
            begin
                #35_000_000;
                $fatal(1, "first AXIS frame timeout; check FIR clock/sample-rate configuration");
            end
        join_any
        disable fork;

        random_ready <= 1'b0;
        if (beat_count != 8192 || complex_count != 4096)
            $fatal(1, "frame length mismatch: beats=%0d complex=%0d",
                   beat_count, complex_count);

        // A coherent 2 MHz CW must become a predominantly DC complex vector.
        dc_power_scaled = sum_i * sum_i + sum_q * sum_q;
        total_power_scaled = 4096 * (sum_i_squared + sum_q_squared);
        if (total_power_scaled < 64'sd100_000_000_000)
            $fatal(1, "CW output energy is unexpectedly small");
        if (dc_power_scaled * 10 < total_power_scaled * 8)
            $fatal(1, "2 MHz CW is not sufficiently concentrated at complex DC");

        repeat (8) @(posedge s_axi_aclk);
        axi_read(5'h10, read_value);
        if (read_value != 32'd1)
            $fatal(1, "FRAME_COUNT=%0d, expected 1", read_value);

        // A long downstream stall must stop the DDC with an explicit fault.
        force_stall <= 1'b1;
        fork
            begin
                wait (dut.overflow_sync2);
            end
            begin
                #35_000_000;
                $fatal(1, "long TREADY stall did not raise overflow/fault");
            end
        join_any
        disable fork;

        axi_read(5'h08, read_value);
        if (!read_value[3] || read_value[0])
            $fatal(1, "fault status is wrong after long stall: STATUS=0x%08h",
                   read_value);

        // RESTART while stopped clears the sticky fault and restores a new
        // frame boundary without emitting a partial pair.
        axi_write(5'h00, 32'h0000_0006); // RESTART | CLEAR_FAULT, RUN=0
        wait (!dut.cfg_busy);
        repeat (16) @(posedge s_axi_aclk);
        axi_read(5'h08, read_value);
        if (read_value[3] || read_value[0])
            $fatal(1, "RESTART did not clear stopped fault state: STATUS=0x%08h",
                   read_value);

        $display("PASS: ADC polarity, PINC commit, CW frame, TLAST, backpressure fault and restart");
        $finish;
    end

endmodule
