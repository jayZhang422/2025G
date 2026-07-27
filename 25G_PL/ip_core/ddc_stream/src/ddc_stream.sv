`timescale 1ns / 1ps

// Fixed-rate streaming DDC for the 5.12006 MSPS ADC path.
//
// AXI-Lite register map (byte offsets):
//   0x00 CTRL            bit0 RUN, bit1 RESTART, bit2 CLEAR_FAULT
//   0x04 PINC            DDS phase-increment shadow register
//   0x08 STATUS          bit0 running, bit1 config busy,
//                        bit2 FIFO prog_full, bit3 overflow/fault sticky
//   0x0c OUTPUT_COUNT    complex samples accepted into the output FIFO
//   0x10 FRAME_COUNT     complete 4096-complex-sample frames
//   0x14 DECIM           read-only 32
//   0x18 SAMPLE_RATE_HZ  read-only 5,120,060
//   0x1c BUILD_ID        read-only build identifier
//
// PINC is applied only by a CTRL write with RESTART set. RESTART also clears
// the FIR state, warmup counter, output FIFO, and frame counters.
module ddc_stream #(
    parameter integer AXI_ADDR_WIDTH = 5,
    parameter logic [31:0] DEFAULT_PINC = 32'h63ff_b333,
    parameter logic [31:0] BUILD_ID = 32'h2026_0727
) (
    input  logic                      clk_adc,
    input  logic                      rst_n,
    input  logic [11:0]               i_adc_raw,
    input  logic                      i_sample_valid,

    input  logic                      s_axi_aclk,
    input  logic                      s_axi_aresetn,
    input  logic [AXI_ADDR_WIDTH-1:0] s_axi_awaddr,
    input  logic                      s_axi_awvalid,
    output logic                      s_axi_awready,
    input  logic [31:0]               s_axi_wdata,
    input  logic [3:0]                s_axi_wstrb,
    input  logic                      s_axi_wvalid,
    output logic                      s_axi_wready,
    output logic [1:0]                s_axi_bresp,
    output logic                      s_axi_bvalid,
    input  logic                      s_axi_bready,
    input  logic [AXI_ADDR_WIDTH-1:0] s_axi_araddr,
    input  logic                      s_axi_arvalid,
    output logic                      s_axi_arready,
    output logic [31:0]               s_axi_rdata,
    output logic [1:0]                s_axi_rresp,
    output logic                      s_axi_rvalid,
    input  logic                      s_axi_rready,

    output logic [15:0]               m_axis_tdata,
    output logic                      m_axis_tvalid,
    input  logic                      m_axis_tready,
    output logic                      m_axis_tlast
);

    localparam integer FRAME_COMPLEX_SAMPLES = 4096;
    localparam integer FIFO_DEPTH = 8192;
    localparam logic [1:0] FIFO_RESET_ASSERT = 2'd0;
    localparam logic [1:0] FIFO_RESET_WAIT   = 2'd1;
    localparam logic [1:0] FIFO_RESET_IDLE   = 2'd2;

    logic [31:0] pinc_shadow;
    logic        run_shadow;
    logic        restart_shadow;
    logic        clear_fault_shadow;
    logic        cfg_req_toggle;
    logic        cfg_ack_adc;
    logic        cfg_ack_sync1;
    logic        cfg_ack_sync2;
    logic        cfg_busy;

    logic [AXI_ADDR_WIDTH-1:0] awaddr_hold;
    logic [31:0]               wdata_hold;
    logic [3:0]                wstrb_hold;
    logic                      aw_hold;
    logic                      w_hold;

    logic        cfg_req_sync1;
    logic        cfg_req_sync2;
    logic        cfg_req_seen;
    logic [1:0]  cfg_settle_count;
    logic        cfg_apply_adc;
    logic        cfg_restart_adc;
    logic        cfg_clear_fault_adc;
    logic        cfg_run_adc;
    logic [31:0] active_pinc_adc;

    logic        running_adc;
    logic        overflow_sticky_adc;
    logic        core_fault;
    logic        core_pair_valid;
    logic signed [15:0] core_i;
    logic signed [15:0] core_q;

    logic [16:0] fifo_din;
    logic [16:0] fifo_dout;
    logic        fifo_wr_en;
    logic        fifo_rd_en;
    logic        fifo_empty;
    logic        fifo_full;
    logic        fifo_prog_full;
    logic        fifo_wr_rst_busy;
    logic        fifo_rd_rst_busy;
    logic [3:0]  fifo_reset_count;
    logic        fifo_rst;
    logic [1:0]  fifo_reset_state;
    logic        fifo_reset_pending;
    logic        run_after_fifo_reset;
    logic        core_restart_adc;
    logic        fifo_wr_en_safe;
    logic        fifo_rd_en_safe;
    (* ASYNC_REG = "TRUE" *) logic fifo_rd_busy_sync1;
    (* ASYNC_REG = "TRUE" *) logic fifo_rd_busy_sync2;

    logic        write_q_pending;
    logic signed [15:0] q_hold;
    logic        q_last_hold;
    logic [11:0] complex_index;
    logic [31:0] output_count_adc;
    logic [31:0] frame_count_adc;
    logic [31:0] output_count_gray_adc;
    logic [31:0] frame_count_gray_adc;

    (* ASYNC_REG = "TRUE" *) logic [31:0] output_gray_sync1;
    (* ASYNC_REG = "TRUE" *) logic [31:0] output_gray_sync2;
    (* ASYNC_REG = "TRUE" *) logic [31:0] frame_gray_sync1;
    (* ASYNC_REG = "TRUE" *) logic [31:0] frame_gray_sync2;
    (* ASYNC_REG = "TRUE" *) logic        running_sync1;
    (* ASYNC_REG = "TRUE" *) logic        running_sync2;
    (* ASYNC_REG = "TRUE" *) logic        overflow_sync1;
    (* ASYNC_REG = "TRUE" *) logic        overflow_sync2;
    (* ASYNC_REG = "TRUE" *) logic        prog_full_sync1;
    (* ASYNC_REG = "TRUE" *) logic        prog_full_sync2;

    logic [31:0] output_count_axi;
    logic [31:0] frame_count_axi;

    function automatic logic [31:0] apply_wstrb(
        input logic [31:0] current_value,
        input logic [31:0] write_value,
        input logic [3:0]  write_strobe
    );
        integer byte_index;
        begin
            apply_wstrb = current_value;
            for (byte_index = 0; byte_index < 4; byte_index = byte_index + 1)
                if (write_strobe[byte_index])
                    apply_wstrb[byte_index*8 +: 8] =
                        write_value[byte_index*8 +: 8];
        end
    endfunction

    function automatic logic [31:0] gray_to_binary(input logic [31:0] gray);
        integer bit_index;
        begin
            gray_to_binary[31] = gray[31];
            for (bit_index = 30; bit_index >= 0; bit_index = bit_index - 1)
                gray_to_binary[bit_index] =
                    gray_to_binary[bit_index + 1] ^ gray[bit_index];
        end
    endfunction

    function automatic logic [31:0] read_register(
        input logic [AXI_ADDR_WIDTH-1:0] address
    );
        begin
            case (address[4:2])
                3'h0: read_register = {31'd0, run_shadow};
                3'h1: read_register = pinc_shadow;
                3'h2: read_register = {28'd0, overflow_sync2,
                                       prog_full_sync2, cfg_busy,
                                       running_sync2};
                3'h3: read_register = output_count_axi;
                3'h4: read_register = frame_count_axi;
                3'h5: read_register = 32'd32;
                3'h6: read_register = 32'd5_120_060;
                3'h7: read_register = BUILD_ID;
                default: read_register = 32'd0;
            endcase
        end
    endfunction

    assign cfg_busy = (cfg_req_toggle != cfg_ack_sync2);
    assign s_axi_awready = !aw_hold && !s_axi_bvalid;
    assign s_axi_wready  = !w_hold  && !s_axi_bvalid;
    assign s_axi_arready = !s_axi_rvalid;
    assign s_axi_rresp   = 2'b00;

    // One outstanding AXI-Lite transaction is sufficient for this register
    // block. AW and W are captured independently as required by AXI-Lite.
    always_ff @(posedge s_axi_aclk) begin
        logic [31:0] merged_value;
        if (!s_axi_aresetn) begin
            pinc_shadow        <= DEFAULT_PINC;
            run_shadow         <= 1'b0;
            restart_shadow     <= 1'b0;
            clear_fault_shadow <= 1'b0;
            cfg_req_toggle     <= 1'b0;
            awaddr_hold        <= '0;
            wdata_hold         <= '0;
            wstrb_hold         <= '0;
            aw_hold            <= 1'b0;
            w_hold             <= 1'b0;
            s_axi_bresp        <= 2'b00;
            s_axi_bvalid       <= 1'b0;
            s_axi_rdata        <= 32'd0;
            s_axi_rvalid       <= 1'b0;
        end else begin
            if (s_axi_awready && s_axi_awvalid) begin
                awaddr_hold <= s_axi_awaddr;
                aw_hold <= 1'b1;
            end

            if (s_axi_wready && s_axi_wvalid) begin
                wdata_hold <= s_axi_wdata;
                wstrb_hold <= s_axi_wstrb;
                w_hold <= 1'b1;
            end

            if (aw_hold && w_hold && !s_axi_bvalid) begin
                s_axi_bresp <= 2'b00;
                case (awaddr_hold[4:2])
                    3'h0: begin
                        if (cfg_busy) begin
                            s_axi_bresp <= 2'b10;
                        end else begin
                            merged_value = apply_wstrb(
                                {31'd0, run_shadow}, wdata_hold, wstrb_hold);
                            run_shadow         <= merged_value[0];
                            restart_shadow     <= merged_value[1];
                            clear_fault_shadow <= merged_value[2];
                            cfg_req_toggle     <= ~cfg_req_toggle;
                        end
                    end
                    3'h1: begin
                        if (cfg_busy)
                            s_axi_bresp <= 2'b10;
                        else
                            pinc_shadow <= apply_wstrb(
                                pinc_shadow, wdata_hold, wstrb_hold);
                    end
                    default: s_axi_bresp <= 2'b10;
                endcase
                aw_hold <= 1'b0;
                w_hold <= 1'b0;
                s_axi_bvalid <= 1'b1;
            end else if (s_axi_bvalid && s_axi_bready) begin
                s_axi_bvalid <= 1'b0;
            end

            if (s_axi_arready && s_axi_arvalid) begin
                s_axi_rdata <= read_register(s_axi_araddr);
                s_axi_rvalid <= 1'b1;
            end else if (s_axi_rvalid && s_axi_rready) begin
                s_axi_rvalid <= 1'b0;
            end
        end
    end

    // Return status and Gray-coded counters to the AXI clock domain.
    always_ff @(posedge s_axi_aclk) begin
        if (!s_axi_aresetn) begin
            cfg_ack_sync1  <= 1'b0;
            cfg_ack_sync2  <= 1'b0;
            running_sync1  <= 1'b0;
            running_sync2  <= 1'b0;
            overflow_sync1 <= 1'b0;
            overflow_sync2 <= 1'b0;
            prog_full_sync1 <= 1'b0;
            prog_full_sync2 <= 1'b0;
            output_gray_sync1 <= 32'd0;
            output_gray_sync2 <= 32'd0;
            frame_gray_sync1  <= 32'd0;
            frame_gray_sync2  <= 32'd0;
        end else begin
            cfg_ack_sync1 <= cfg_ack_adc;
            cfg_ack_sync2 <= cfg_ack_sync1;
            running_sync1 <= running_adc;
            running_sync2 <= running_sync1;
            overflow_sync1 <= overflow_sticky_adc;
            overflow_sync2 <= overflow_sync1;
            prog_full_sync1 <= fifo_prog_full;
            prog_full_sync2 <= prog_full_sync1;
            output_gray_sync1 <= output_count_gray_adc;
            output_gray_sync2 <= output_gray_sync1;
            frame_gray_sync1 <= frame_count_gray_adc;
            frame_gray_sync2 <= frame_gray_sync1;
        end
    end

    always_comb begin
        output_count_axi = gray_to_binary(output_gray_sync2);
        frame_count_axi  = gray_to_binary(frame_gray_sync2);
    end

    // The request toggle and its multi-bit shadow bus remain stable until the
    // ADC domain acknowledges them. Two extra clocks settle the shadow bus.
    always_ff @(posedge clk_adc or negedge rst_n) begin
        if (!rst_n) begin
            cfg_req_sync1       <= 1'b0;
            cfg_req_sync2       <= 1'b0;
            cfg_req_seen        <= 1'b0;
            cfg_settle_count    <= 2'd0;
            cfg_apply_adc       <= 1'b0;
            cfg_restart_adc     <= 1'b0;
            cfg_clear_fault_adc <= 1'b0;
            cfg_run_adc         <= 1'b0;
            active_pinc_adc     <= DEFAULT_PINC;
            cfg_ack_adc         <= 1'b0;
        end else begin
            cfg_req_sync1 <= cfg_req_toggle;
            cfg_req_sync2 <= cfg_req_sync1;
            cfg_apply_adc       <= 1'b0;
            cfg_restart_adc     <= 1'b0;
            cfg_clear_fault_adc <= 1'b0;

            if (cfg_req_sync2 != cfg_req_seen) begin
                cfg_req_seen <= cfg_req_sync2;
                cfg_settle_count <= 2'd2;
            end else if (cfg_settle_count != 0) begin
                cfg_settle_count <= cfg_settle_count - 1'b1;
                if (cfg_settle_count == 1) begin
                    cfg_run_adc <= run_shadow;
                    cfg_restart_adc <= restart_shadow;
                    cfg_clear_fault_adc <= clear_fault_shadow;
                    if (restart_shadow)
                        active_pinc_adc <= pinc_shadow;
                    cfg_apply_adc <= 1'b1;
                end
            end

            // Acknowledge only after the consumer has seen cfg_apply_adc.
            if (cfg_apply_adc)
                cfg_ack_adc <= cfg_req_seen;
        end
    end

    ddc_stream_core u_core (
        .clk_adc(clk_adc),
        .rst_n(rst_n),
        .i_adc_raw(i_adc_raw),
        .i_sample_valid(i_sample_valid),
        .i_run(running_adc),
        .i_restart(core_restart_adc),
        .i_pinc(active_pinc_adc),
        .o_pair_valid(core_pair_valid),
        .o_i(core_i),
        .o_q(core_q),
        .o_fault(core_fault)
    );

    assign output_count_gray_adc =
        output_count_adc ^ (output_count_adc >> 1);
    assign frame_count_gray_adc =
        frame_count_adc ^ (frame_count_adc >> 1);

    // Serialize each complex point as I then Q. PROG_FULL is set early enough
    // to reserve both words, so a half I/Q pair is never written.
    always_ff @(posedge clk_adc or negedge rst_n) begin
        if (!rst_n) begin
            running_adc         <= 1'b0;
            overflow_sticky_adc <= 1'b0;
            fifo_wr_en           <= 1'b0;
            fifo_din             <= 17'd0;
            write_q_pending      <= 1'b0;
            q_hold               <= 16'sd0;
            q_last_hold          <= 1'b0;
            complex_index        <= 12'd0;
            output_count_adc     <= 32'd0;
            frame_count_adc      <= 32'd0;
            fifo_reset_count     <= 4'd8;
            fifo_rst             <= 1'b1;
            fifo_reset_state     <= FIFO_RESET_ASSERT;
            fifo_reset_pending   <= 1'b0;
            run_after_fifo_reset <= 1'b0;
            core_restart_adc     <= 1'b1;
            fifo_rd_busy_sync1   <= 1'b1;
            fifo_rd_busy_sync2   <= 1'b1;
        end else begin
            fifo_wr_en <= 1'b0;
            core_restart_adc <= 1'b0;
            fifo_rd_busy_sync1 <= fifo_rd_rst_busy;
            fifo_rd_busy_sync2 <= fifo_rd_busy_sync1;

            if (cfg_apply_adc) begin
                if (cfg_restart_adc) begin
                    fifo_reset_pending <= 1'b1;
                    run_after_fifo_reset <= cfg_run_adc;
                    running_adc <= 1'b0;
                end else if ((fifo_reset_state == FIFO_RESET_IDLE) &&
                             !fifo_reset_pending) begin
                    running_adc <= cfg_run_adc;
                end else begin
                    run_after_fifo_reset <= cfg_run_adc;
                end
            end

            if (cfg_clear_fault_adc || cfg_restart_adc)
                overflow_sticky_adc <= 1'b0;

            case (fifo_reset_state)
                FIFO_RESET_ASSERT: begin
                    running_adc <= 1'b0;
                    core_restart_adc <= 1'b1;
                    if (fifo_reset_count > 1) begin
                        fifo_reset_count <= fifo_reset_count - 1'b1;
                    end else begin
                        fifo_reset_count <= 4'd0;
                        fifo_rst <= 1'b0;
                        fifo_reset_state <= FIFO_RESET_WAIT;
                    end
                end

                FIFO_RESET_WAIT: begin
                    running_adc <= 1'b0;
                    if (!fifo_wr_rst_busy && !fifo_rd_busy_sync2) begin
                        fifo_reset_state <= FIFO_RESET_IDLE;
                        if (!fifo_reset_pending)
                            running_adc <= run_after_fifo_reset;
                    end
                end

                default: begin // FIFO_RESET_IDLE
                    if (fifo_reset_pending) begin
                        // XPM requires the previous reset sequence to finish
                        // before a new reset is asserted.
                        fifo_reset_pending <= 1'b0;
                        fifo_rst <= 1'b1;
                        fifo_reset_count <= 4'd8;
                        fifo_reset_state <= FIFO_RESET_ASSERT;
                        core_restart_adc <= 1'b1;
                        running_adc <= 1'b0;
                        write_q_pending <= 1'b0;
                        complex_index <= 12'd0;
                        output_count_adc <= 32'd0;
                        frame_count_adc <= 32'd0;
                    end else if (write_q_pending) begin
                        // The preceding cycle placed I on fifo_din; this cycle
                        // queues Q.
                        fifo_din <= {q_last_hold, q_hold};
                        fifo_wr_en <= 1'b1;
                        write_q_pending <= 1'b0;
                    end else if (core_pair_valid && running_adc &&
                                 !(cfg_apply_adc && !cfg_run_adc)) begin
                        if (fifo_prog_full || fifo_full || fifo_wr_rst_busy) begin
                            overflow_sticky_adc <= 1'b1;
                            running_adc <= 1'b0;
                        end else begin
                            fifo_din <= {1'b0, core_i};
                            fifo_wr_en <= 1'b1;
                            q_hold <= core_q;
                            q_last_hold <= (complex_index ==
                                            FRAME_COMPLEX_SAMPLES - 1);
                            write_q_pending <= 1'b1;
                            output_count_adc <= output_count_adc + 1'b1;
                            if (complex_index == FRAME_COMPLEX_SAMPLES - 1) begin
                                complex_index <= 12'd0;
                                frame_count_adc <= frame_count_adc + 1'b1;
                            end else begin
                                complex_index <= complex_index + 1'b1;
                            end
                        end
                    end
                end
            endcase

            // A core input-overrun or unexpected pair overlap is explicit.
            if ((fifo_reset_state == FIFO_RESET_IDLE) &&
                !fifo_reset_pending && !cfg_restart_adc &&
                (core_fault || (core_pair_valid && write_q_pending))) begin
                overflow_sticky_adc <= 1'b1;
                running_adc <= 1'b0;
            end
        end
    end

    assign fifo_wr_en_safe = fifo_wr_en && rst_n && !fifo_rst &&
                             !fifo_wr_rst_busy;

    xpm_fifo_async #(
        .CDC_SYNC_STAGES(2),
        .DOUT_RESET_VALUE("0"),
        .ECC_MODE("no_ecc"),
        .FIFO_MEMORY_TYPE("block"),
        .FIFO_READ_LATENCY(0),
        .FIFO_WRITE_DEPTH(FIFO_DEPTH),
        .FULL_RESET_VALUE(0),
        .PROG_EMPTY_THRESH(10),
        .PROG_FULL_THRESH(FIFO_DEPTH - 5),
        .RD_DATA_COUNT_WIDTH(14),
        .READ_DATA_WIDTH(17),
        .READ_MODE("fwft"),
        .RELATED_CLOCKS(0),
        // Vivado 2020.2 reports false SLEEP/EMPTY assertions at startup;
        // reset ordering is enforced by fifo_reset_state above.
        .SIM_ASSERT_CHK(0),
        .USE_ADV_FEATURES("0707"),
        .WAKEUP_TIME(0),
        .WRITE_DATA_WIDTH(17),
        .WR_DATA_COUNT_WIDTH(14)
    ) u_output_fifo (
        .almost_empty(),
        .almost_full(),
        .data_valid(),
        .dbiterr(),
        .dout(fifo_dout),
        .empty(fifo_empty),
        .full(fifo_full),
        .overflow(),
        .prog_empty(),
        .prog_full(fifo_prog_full),
        .rd_data_count(),
        .rd_rst_busy(fifo_rd_rst_busy),
        .sbiterr(),
        .underflow(),
        .wr_ack(),
        .wr_data_count(),
        .wr_rst_busy(fifo_wr_rst_busy),
        .din(fifo_din),
        .injectdbiterr(1'b0),
        .injectsbiterr(1'b0),
        .rd_clk(s_axi_aclk),
        .rd_en(fifo_rd_en_safe),
        .rst(fifo_rst),
        .sleep(1'b0),
        .wr_clk(clk_adc),
        .wr_en(fifo_wr_en_safe)
    );

    assign m_axis_tdata  = fifo_dout[15:0];
    assign m_axis_tlast  = fifo_dout[16];
    assign m_axis_tvalid = rst_n && s_axi_aresetn && !fifo_rst &&
                           !fifo_empty && !fifo_rd_rst_busy;
    assign fifo_rd_en    = m_axis_tvalid && m_axis_tready;
    assign fifo_rd_en_safe = fifo_rd_en && rst_n && s_axi_aresetn &&
                             !fifo_rst && !fifo_rd_rst_busy;

endmodule


module ddc_stream_core #(
    parameter integer DDS_LATENCY = 8,
    parameter integer MAX_DDS_LATENCY = 16,
    parameter integer FIR_WARMUP_OUTPUTS = 16
) (
    input  logic               clk_adc,
    input  logic               rst_n,
    input  logic [11:0]        i_adc_raw,
    input  logic               i_sample_valid,
    input  logic               i_run,
    input  logic               i_restart,
    input  logic [31:0]        i_pinc,
    output logic               o_pair_valid,
    output logic signed [15:0] o_i,
    output logic signed [15:0] o_q,
    output logic               o_fault
);

    localparam logic [11:0] ADC_MIDSCALE = 12'd2048;

    logic [63:0] dds_config_tdata;
    logic        dds_config_tvalid;
    logic        dds_config_pending;
    logic [31:0] dds_tdata;
    logic        dds_tvalid;
    logic        dds_aresetn;
    logic signed [15:0] lo_cos;
    logic signed [15:0] lo_sin;

    logic signed [12:0] adc_centered;
    logic signed [12:0] adc_pipe [0:MAX_DDS_LATENCY];
    logic               valid_pipe [0:MAX_DDS_LATENCY];
    integer pipe_index;

    logic signed [28:0] i_product;
    logic signed [28:0] q_product;
    logic signed [15:0] mix_i;
    logic signed [15:0] mix_q;
    logic               mix_valid;

    logic        fir_i_s_tready;
    logic        fir_q_s_tready;
    logic        fir_s_tvalid;
    logic [23:0] fir_i_s_tdata;
    logic [23:0] fir_q_s_tdata;
    logic        fir_i_m_tvalid;
    logic        fir_q_m_tvalid;
    logic [39:0] fir_i_m_tdata;
    logic [39:0] fir_q_m_tdata;
    logic [4:0]  warmup_count;

    function automatic logic signed [15:0] product_to_s16(
        input logic signed [28:0] value
    );
        logic signed [29:0] extended_value;
        logic signed [29:0] rounded_value;
        begin
            extended_value = {value[28], value};
            if (extended_value >= 0)
                rounded_value = (extended_value + 30'sd16384) >>> 15;
            else
                rounded_value = -(((-extended_value) + 30'sd16384) >>> 15);

            if (rounded_value > 30'sd32767)
                product_to_s16 = 16'sh7fff;
            else if (rounded_value < -30'sd32768)
                product_to_s16 = -16'sd32768;
            else
                product_to_s16 = rounded_value[15:0];
        end
    endfunction

    // The supplied 18-bit coefficient set has unity gain represented by
    // 2^17 (sum = 131072), so full-precision FIR output is Q17.
    function automatic logic signed [15:0] fir_to_s16(
        input logic signed [35:0] value
    );
        logic signed [36:0] extended_value;
        logic signed [36:0] rounded_value;
        begin
            extended_value = {value[35], value};
            if (extended_value >= 0)
                rounded_value = (extended_value + 37'sd65536) >>> 17;
            else
                rounded_value = -(((-extended_value) + 37'sd65536) >>> 17);

            if (rounded_value > 37'sd32767)
                fir_to_s16 = 16'sh7fff;
            else if (rounded_value < -37'sd32768)
                fir_to_s16 = -16'sd32768;
            else
                fir_to_s16 = rounded_value[15:0];
        end
    endfunction

    // Board polarity: increasing ADC code is negative signal voltage.
    assign adc_centered = $signed({1'b0, ADC_MIDSCALE}) -
                          $signed({1'b0, i_adc_raw});
    assign lo_cos = $signed(dds_tdata[15:0]);
    assign lo_sin = $signed(dds_tdata[31:16]);
    assign i_product = adc_pipe[DDS_LATENCY] * lo_cos;
    assign q_product = -(adc_pipe[DDS_LATENCY] * lo_sin);
    assign mix_i = product_to_s16(i_product);
    assign mix_q = product_to_s16(q_product);
    assign mix_valid = i_run && !o_fault && dds_tvalid &&
                       valid_pipe[DDS_LATENCY];

    assign fir_s_tvalid = mix_valid && fir_i_s_tready && fir_q_s_tready;
    // FIR Compiler uses an 18-bit signed sample in a byte-padded 24-bit TDATA.
    assign fir_i_s_tdata = {{8{mix_i[15]}}, mix_i};
    assign fir_q_s_tdata = {{8{mix_q[15]}}, mix_q};
    assign dds_aresetn = rst_n && !i_restart;

    dds_iq_lo u_dds_iq_lo (
        .aclk(clk_adc),
        .aresetn(dds_aresetn),
        .s_axis_config_tvalid(dds_config_tvalid),
        .s_axis_config_tdata(dds_config_tdata),
        .m_axis_data_tvalid(dds_tvalid),
        .m_axis_data_tdata(dds_tdata),
        .m_axis_phase_tvalid(),
        .m_axis_phase_tdata()
    );

    fir_ddc_i u_fir_ddc_i (
        .aresetn(dds_aresetn),
        .aclk(clk_adc),
        .s_axis_data_tvalid(fir_s_tvalid),
        .s_axis_data_tready(fir_i_s_tready),
        .s_axis_data_tdata(fir_i_s_tdata),
        .m_axis_data_tvalid(fir_i_m_tvalid),
        .m_axis_data_tready(1'b1),
        .m_axis_data_tdata(fir_i_m_tdata)
    );

    fir_ddc_q u_fir_ddc_q (
        .aresetn(dds_aresetn),
        .aclk(clk_adc),
        .s_axis_data_tvalid(fir_s_tvalid),
        .s_axis_data_tready(fir_q_s_tready),
        .s_axis_data_tdata(fir_q_s_tdata),
        .m_axis_data_tvalid(fir_q_m_tvalid),
        .m_axis_data_tready(1'b1),
        .m_axis_data_tdata(fir_q_m_tdata)
    );

    // Send a new DDS configuration one clock after RESTART releases reset.
    always_ff @(posedge clk_adc or negedge rst_n) begin
        if (!rst_n) begin
            dds_config_tdata   <= 64'd0;
            dds_config_tvalid  <= 1'b0;
            dds_config_pending <= 1'b0;
            for (pipe_index = 0; pipe_index <= MAX_DDS_LATENCY;
                 pipe_index = pipe_index + 1) begin
                adc_pipe[pipe_index] <= 13'sd0;
                valid_pipe[pipe_index] <= 1'b0;
            end
        end else begin
            dds_config_tvalid <= 1'b0;
            if (i_restart) begin
                dds_config_pending <= 1'b1;
                for (pipe_index = 0; pipe_index <= MAX_DDS_LATENCY;
                     pipe_index = pipe_index + 1) begin
                    adc_pipe[pipe_index] <= 13'sd0;
                    valid_pipe[pipe_index] <= 1'b0;
                end
            end else begin
                if (dds_config_pending) begin
                    dds_config_tdata <= {32'd0, i_pinc};
                    dds_config_tvalid <= 1'b1;
                    dds_config_pending <= 1'b0;
                end
                adc_pipe[0] <= adc_centered;
                valid_pipe[0] <= i_sample_valid && i_run;
                for (pipe_index = 1; pipe_index <= MAX_DDS_LATENCY;
                     pipe_index = pipe_index + 1) begin
                    adc_pipe[pipe_index] <= adc_pipe[pipe_index - 1];
                    valid_pipe[pipe_index] <= valid_pipe[pipe_index - 1];
                end
            end
        end
    end

    always_ff @(posedge clk_adc or negedge rst_n) begin
        if (!rst_n) begin
            warmup_count <= 5'd0;
            o_pair_valid <= 1'b0;
            o_i <= 16'sd0;
            o_q <= 16'sd0;
            o_fault <= 1'b0;
        end else begin
            o_pair_valid <= 1'b0;
            if (i_restart) begin
                warmup_count <= 5'd0;
                o_fault <= 1'b0;
            end else begin
                if (mix_valid && !(fir_i_s_tready && fir_q_s_tready))
                    o_fault <= 1'b1;
                if (fir_i_m_tvalid != fir_q_m_tvalid)
                    o_fault <= 1'b1;

                if (fir_i_m_tvalid && fir_q_m_tvalid && i_run && !o_fault) begin
                    if (warmup_count < FIR_WARMUP_OUTPUTS)
                        warmup_count <= warmup_count + 1'b1;
                    else begin
                        o_i <= fir_to_s16($signed(fir_i_m_tdata[35:0]));
                        o_q <= fir_to_s16($signed(fir_q_m_tdata[35:0]));
                        o_pair_valid <= 1'b1;
                    end
                end
            end
        end
    end

endmodule
