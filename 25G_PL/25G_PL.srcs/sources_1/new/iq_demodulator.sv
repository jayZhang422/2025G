// AXI4-Lite controlled, ADC-clock-domain I/Q lock-in detector.
//
// A configuration can describe either one fixed-frequency detector or a
// sequential frequency scan.  Each completed integration is held until
// software acknowledges it, so a slow AXI reader cannot silently lose a bin.
// Magnitude, phase, peak selection, and waveform classification remain PS work.
//
// AXI register map (byte offsets):
//   0x00 CTRL       bit 0 enable, bit 1 commit toggle, bit 2 scan enable,
//                   bit 3 scan repeat
//   0x04 PINC       initial DDS phase increment
//   0x08 POFFSET    DDS phase offset used for every scan point
//   0x0C WINDOW     non-zero ADC-sample integration length
//   0x10 STATUS     bit 0 result pending (W1C), bit 1 config acknowledged,
//                   bit 2 config busy, bit 3 ADC waits for result W1C
//   0x14 RESULT_SEQ monotonically increasing completed-result sequence
//   0x18/0x1C I_SUM low/high 16-bit words
//   0x20/0x24 Q_SUM low/high 16-bit words
//   0x28 SAMPLE_COUNT
//   0x2C RESULT_PINC phase increment used for this result
//   0x30 SCAN_STEP  signed phase-increment delta between scan points
//   0x34 SCAN_COUNT number of scan points; zero is treated as one point
//
// Software writes all shadow registers before committing CTRL.  In scan mode,
// it reads one result, records RESULT_PINC with I/Q, then writes STATUS[0]=1
// to advance to the next point.  A scan point is never changed mid-window.
module iq_demodulator #(
    parameter int ADC_WIDTH       = 12,
    parameter int LO_WIDTH        = 16,
    parameter int PHASE_WIDTH     = 32,
    parameter int ACC_WIDTH       = 48,
    parameter int DDS_LATENCY     = 8,
    parameter int MAX_DDS_LATENCY = 16,
    parameter int AXI_ADDR_WIDTH  = 6
) (
    input  logic                   clk_adc,
    input  logic                   rst_n,
    input  logic                   i_sample_valid,
    input  logic [ADC_WIDTH-1:0]   i_adc_raw,

    input  logic                   s_axi_aclk,
    input  logic                   s_axi_aresetn,
    input  logic [AXI_ADDR_WIDTH-1:0] s_axi_awaddr,
    input  logic                   s_axi_awvalid,
    output logic                   s_axi_awready,
    input  logic [31:0]            s_axi_wdata,
    input  logic [3:0]             s_axi_wstrb,
    input  logic                   s_axi_wvalid,
    output logic                   s_axi_wready,
    output logic [1:0]             s_axi_bresp,
    output logic                   s_axi_bvalid,
    input  logic                   s_axi_bready,
    input  logic [AXI_ADDR_WIDTH-1:0] s_axi_araddr,
    input  logic                   s_axi_arvalid,
    output logic                   s_axi_arready,
    output logic [31:0]            s_axi_rdata,
    output logic [1:0]             s_axi_rresp,
    output logic                   s_axi_rvalid,
    input  logic                   s_axi_rready,

    output logic signed [ACC_WIDTH-1:0] o_i_sum,
    output logic signed [ACC_WIDTH-1:0] o_q_sum,
    output logic [15:0]                 o_sample_count,
    output logic                        o_ready,
    output logic [31:0]                 o_result_seq,
    output logic                        o_irq
);

    localparam logic [ADC_WIDTH-1:0] ADC_MIDSCALE =
        {1'b1, {(ADC_WIDTH - 1){1'b0}}};

    logic [PHASE_WIDTH-1:0] cfg_pinc_shadow;
    logic [PHASE_WIDTH-1:0] cfg_poffset_shadow;
    logic [PHASE_WIDTH-1:0] cfg_scan_step_shadow;
    logic [15:0]            cfg_window_shadow;
    logic [15:0]            cfg_scan_count_shadow;
    logic                   cfg_enable_shadow;
    logic                   cfg_scan_enable_shadow;
    logic                   cfg_scan_repeat_shadow;
    logic                   cfg_req_toggle;
    logic                   result_ack_toggle;

    logic [AXI_ADDR_WIDTH-1:0] awaddr_hold;
    logic [31:0]               wdata_hold;
    logic [3:0]                wstrb_hold;
    logic                      aw_hold;
    logic                      w_hold;

    logic result_toggle_adc;
    logic result_toggle_sync1;
    logic result_toggle_sync2;
    logic result_toggle_seen;
    logic signed [ACC_WIDTH-1:0] axi_i_sum;
    logic signed [ACC_WIDTH-1:0] axi_q_sum;
    logic [15:0] axi_sample_count;
    logic [31:0] axi_result_seq;
    logic [PHASE_WIDTH-1:0] axi_result_pinc;
    logic result_pending;
    logic cfg_ack_sync1;
    logic cfg_ack_sync2;
    logic cfg_busy;
    logic result_waiting_sync1;
    logic result_waiting_sync2;

    logic cfg_req_sync1;
    logic cfg_req_sync2;
    logic cfg_req_seen;
    logic [1:0] cfg_settle_count;
    logic cfg_apply_pending;
    logic cfg_apply_adc;
    logic cfg_enable_adc;
    logic cfg_scan_enable_adc;
    logic cfg_scan_repeat_adc;
    logic cfg_ack_adc;
    logic [PHASE_WIDTH-1:0] cfg_pinc_adc;
    logic [PHASE_WIDTH-1:0] cfg_poffset_adc;
    logic [PHASE_WIDTH-1:0] cfg_scan_step_adc;
    logic [15:0] cfg_window_adc;
    logic [15:0] cfg_scan_count_adc;

    logic result_ack_sync1;
    logic result_ack_sync2;
    logic result_ack_seen;
    logic result_waiting_ack_adc;
    logic [PHASE_WIDTH-1:0] o_result_pinc;
    logic measurement_enabled_adc;

    function automatic [31:0] merge_wstrb(
        input [31:0] old_value,
        input [31:0] new_value,
        input [3:0] strobe
    );
        integer byte_index;
        begin
            merge_wstrb = old_value;
            for (byte_index = 0; byte_index < 4; byte_index = byte_index + 1) begin
                if (strobe[byte_index])
                    merge_wstrb[byte_index*8 +: 8] = new_value[byte_index*8 +: 8];
            end
        end
    endfunction

    function automatic [31:0] read_register(
        input [AXI_ADDR_WIDTH-1:0] address
    );
        begin
            case (address[5:2])
                4'h0: read_register = {28'd0, cfg_scan_repeat_shadow,
                                        cfg_scan_enable_shadow, 1'b0,
                                        cfg_enable_shadow};
                4'h1: read_register = cfg_pinc_shadow;
                4'h2: read_register = cfg_poffset_shadow;
                4'h3: read_register = {16'd0, cfg_window_shadow};
                4'h4: read_register = {28'd0, result_waiting_sync2, cfg_busy,
                                        (cfg_ack_sync2 == cfg_req_toggle),
                                        result_pending};
                4'h5: read_register = axi_result_seq;
                4'h6: read_register = axi_i_sum[31:0];
                4'h7: read_register = {{16{axi_i_sum[ACC_WIDTH-1]}},
                                        axi_i_sum[ACC_WIDTH-1:32]};
                4'h8: read_register = axi_q_sum[31:0];
                4'h9: read_register = {{16{axi_q_sum[ACC_WIDTH-1]}},
                                        axi_q_sum[ACC_WIDTH-1:32]};
                4'hA: read_register = {16'd0, axi_sample_count};
                4'hB: read_register = axi_result_pinc;
                4'hC: read_register = cfg_scan_step_shadow;
                4'hD: read_register = {16'd0, cfg_scan_count_shadow};
                default: read_register = 32'd0;
            endcase
        end
    endfunction

    assign s_axi_bresp = 2'b00;
    assign s_axi_rresp = 2'b00;
    assign o_irq = result_pending;
    assign cfg_busy = (cfg_ack_sync2 != cfg_req_toggle);

    // AXI4-Lite slave. A result toggle is synchronized before its stable
    // payload is captured. W1C acknowledges that exact held result.
    always_ff @(posedge s_axi_aclk) begin
        if (!s_axi_aresetn) begin
            s_axi_awready <= 1'b0;
            s_axi_wready <= 1'b0;
            s_axi_bvalid <= 1'b0;
            s_axi_arready <= 1'b0;
            s_axi_rvalid <= 1'b0;
            s_axi_rdata <= '0;
            awaddr_hold <= '0;
            wdata_hold <= '0;
            wstrb_hold <= '0;
            aw_hold <= 1'b0;
            w_hold <= 1'b0;
            cfg_pinc_shadow <= '0;
            cfg_poffset_shadow <= '0;
            cfg_scan_step_shadow <= '0;
            cfg_window_shadow <= 16'd4096;
            cfg_scan_count_shadow <= 16'd1;
            cfg_enable_shadow <= 1'b0;
            cfg_scan_enable_shadow <= 1'b0;
            cfg_scan_repeat_shadow <= 1'b0;
            cfg_req_toggle <= 1'b0;
            result_ack_toggle <= 1'b0;
            result_toggle_sync1 <= 1'b0;
            result_toggle_sync2 <= 1'b0;
            result_toggle_seen <= 1'b0;
            axi_i_sum <= '0;
            axi_q_sum <= '0;
            axi_sample_count <= '0;
            axi_result_seq <= '0;
            axi_result_pinc <= '0;
            result_pending <= 1'b0;
            cfg_ack_sync1 <= 1'b0;
            cfg_ack_sync2 <= 1'b0;
            result_waiting_sync1 <= 1'b0;
            result_waiting_sync2 <= 1'b0;
        end else begin
            s_axi_awready <= 1'b0;
            s_axi_wready <= 1'b0;
            s_axi_arready <= 1'b0;
            result_toggle_sync1 <= result_toggle_adc;
            result_toggle_sync2 <= result_toggle_sync1;
            cfg_ack_sync1 <= cfg_ack_adc;
            cfg_ack_sync2 <= cfg_ack_sync1;
            result_waiting_sync1 <= result_waiting_ack_adc;
            result_waiting_sync2 <= result_waiting_sync1;

            if (result_toggle_sync2 != result_toggle_seen) begin
                result_toggle_seen <= result_toggle_sync2;
                axi_i_sum <= o_i_sum;
                axi_q_sum <= o_q_sum;
                axi_sample_count <= o_sample_count;
                axi_result_seq <= o_result_seq;
                axi_result_pinc <= o_result_pinc;
                result_pending <= 1'b1;
            end

            if (!aw_hold && !s_axi_bvalid && s_axi_awvalid) begin
                s_axi_awready <= 1'b1;
                awaddr_hold <= s_axi_awaddr;
                aw_hold <= 1'b1;
            end
            if (!w_hold && !s_axi_bvalid && s_axi_wvalid) begin
                s_axi_wready <= 1'b1;
                wdata_hold <= s_axi_wdata;
                wstrb_hold <= s_axi_wstrb;
                w_hold <= 1'b1;
            end
            if (aw_hold && w_hold && !s_axi_bvalid) begin
                aw_hold <= 1'b0;
                w_hold <= 1'b0;
                s_axi_bvalid <= 1'b1;
                case (awaddr_hold[5:2])
                    4'h0: if (wstrb_hold[0]) begin
                        cfg_enable_shadow <= wdata_hold[0];
                        cfg_scan_enable_shadow <= wdata_hold[2];
                        cfg_scan_repeat_shadow <= wdata_hold[3];
                        if (wdata_hold[1])
                            cfg_req_toggle <= ~cfg_req_toggle;
                    end
                    4'h1: if (!cfg_busy) cfg_pinc_shadow <= merge_wstrb(
                        cfg_pinc_shadow, wdata_hold, wstrb_hold);
                    4'h2: if (!cfg_busy) cfg_poffset_shadow <= merge_wstrb(
                        cfg_poffset_shadow, wdata_hold, wstrb_hold);
                    4'h3: if (!cfg_busy) cfg_window_shadow <= merge_wstrb(
                        {16'd0, cfg_window_shadow}, wdata_hold, wstrb_hold);
                    4'h4: if (wstrb_hold[0] && wdata_hold[0] &&
                               result_pending &&
                               (result_toggle_sync2 == result_toggle_seen)) begin
                        result_pending <= 1'b0;
                        result_ack_toggle <= ~result_ack_toggle;
                    end
                    4'hC: if (!cfg_busy) cfg_scan_step_shadow <= merge_wstrb(
                        cfg_scan_step_shadow, wdata_hold, wstrb_hold);
                    4'hD: if (!cfg_busy) cfg_scan_count_shadow <= merge_wstrb(
                        {16'd0, cfg_scan_count_shadow}, wdata_hold, wstrb_hold);
                    default: begin end
                endcase
            end
            if (s_axi_bvalid && s_axi_bready)
                s_axi_bvalid <= 1'b0;

            if (!s_axi_rvalid && s_axi_arvalid) begin
                s_axi_arready <= 1'b1;
                s_axi_rdata <= read_register(s_axi_araddr);
                s_axi_rvalid <= 1'b1;
            end else if (s_axi_rvalid && s_axi_rready) begin
                s_axi_rvalid <= 1'b0;
            end
        end
    end

    // A committed AXI configuration is held stable until its request has been
    // acknowledged. The two ADC clocks after the synchronized request settle
    // the multi-bit shadow bus before it is sampled.
    always_ff @(posedge clk_adc or negedge rst_n) begin
        if (!rst_n) begin
            cfg_req_sync1 <= 1'b0;
            cfg_req_sync2 <= 1'b0;
            cfg_req_seen <= 1'b0;
            cfg_settle_count <= '0;
            cfg_apply_pending <= 1'b0;
            cfg_apply_adc <= 1'b0;
            cfg_enable_adc <= 1'b0;
            cfg_scan_enable_adc <= 1'b0;
            cfg_scan_repeat_adc <= 1'b0;
            cfg_ack_adc <= 1'b0;
            cfg_pinc_adc <= '0;
            cfg_poffset_adc <= '0;
            cfg_scan_step_adc <= '0;
            cfg_window_adc <= 16'd4096;
            cfg_scan_count_adc <= 16'd1;
        end else begin
            cfg_req_sync1 <= cfg_req_toggle;
            cfg_req_sync2 <= cfg_req_sync1;
            cfg_apply_adc <= 1'b0;
            if (cfg_req_sync2 != cfg_req_seen) begin
                cfg_req_seen <= cfg_req_sync2;
                cfg_settle_count <= 2'd2;
            end else if (cfg_settle_count != 0) begin
                cfg_settle_count <= cfg_settle_count - 1'b1;
                if (cfg_settle_count == 1) begin
                    cfg_pinc_adc <= cfg_pinc_shadow;
                    cfg_poffset_adc <= cfg_poffset_shadow;
                    cfg_scan_step_adc <= cfg_scan_step_shadow;
                    cfg_window_adc <= cfg_window_shadow;
                    cfg_scan_count_adc <= (cfg_scan_count_shadow == 0) ?
                                          16'd1 : cfg_scan_count_shadow;
                    cfg_enable_adc <= cfg_enable_shadow;
                    cfg_scan_enable_adc <= cfg_scan_enable_shadow;
                    cfg_scan_repeat_adc <= cfg_scan_repeat_shadow;
                    cfg_apply_pending <= 1'b1;
                    cfg_ack_adc <= cfg_req_sync2;
                end
            end else if (cfg_apply_pending) begin
                cfg_apply_adc <= 1'b1;
                cfg_apply_pending <= 1'b0;
            end
        end
    end

    logic [63:0] config_tdata;
    logic config_tvalid;
    logic [2*LO_WIDTH-1:0] lo_tdata;
    logic lo_tvalid;
    logic signed [LO_WIDTH-1:0] lo_cos;
    logic signed [LO_WIDTH-1:0] lo_sin;
    logic signed [ADC_WIDTH:0] adc_pipe [0:MAX_DDS_LATENCY];
    logic valid_pipe [0:MAX_DDS_LATENCY];
    logic signed [ADC_WIDTH:0] adc_centered;
    logic signed [ADC_WIDTH+LO_WIDTH:0] i_product;
    logic signed [ADC_WIDTH+LO_WIDTH:0] q_product;
    logic signed [ACC_WIDTH-1:0] i_acc;
    logic signed [ACC_WIDTH-1:0] q_acc;
    logic [15:0] sample_count;
    logic [7:0] warmup_count;
    logic [PHASE_WIDTH-1:0] active_pinc;
    logic [15:0] scan_index;
    integer k;

    assign lo_cos = lo_tdata[LO_WIDTH-1:0];
    assign lo_sin = lo_tdata[2*LO_WIDTH-1:LO_WIDTH];
    assign adc_centered = $signed({1'b0, i_adc_raw}) -
                          $signed({1'b0, ADC_MIDSCALE});
    assign i_product = adc_pipe[DDS_LATENCY] * lo_cos;
    assign q_product = adc_pipe[DDS_LATENCY] * lo_sin;

    dds_iq_lo u_dds_iq_lo (
        .aclk(clk_adc),
        .s_axis_config_tvalid(config_tvalid),
        .s_axis_config_tdata(config_tdata),
        .m_axis_data_tvalid(lo_tvalid),
        .m_axis_data_tdata(lo_tdata),
        .m_axis_phase_tvalid(),
        .m_axis_phase_tdata()
    );

    always_ff @(posedge clk_adc or negedge rst_n) begin
        if (!rst_n) begin
            config_tdata <= '0;
            config_tvalid <= 1'b0;
            for (k = 0; k <= MAX_DDS_LATENCY; k = k + 1) begin
                adc_pipe[k] <= '0;
                valid_pipe[k] <= 1'b0;
            end
        end else begin
            config_tvalid <= cfg_apply_adc ||
                             (result_waiting_ack_adc &&
                              (result_ack_sync2 != result_ack_seen) &&
                              measurement_enabled_adc &&
                              (!cfg_scan_enable_adc ||
                               (scan_index + 1 < cfg_scan_count_adc) ||
                               cfg_scan_repeat_adc));
            if (cfg_apply_adc)
                config_tdata <= {cfg_poffset_adc, cfg_pinc_adc};
            else if (result_waiting_ack_adc &&
                     (result_ack_sync2 != result_ack_seen) &&
                     measurement_enabled_adc &&
                     (!cfg_scan_enable_adc ||
                      (scan_index + 1 < cfg_scan_count_adc) ||
                      cfg_scan_repeat_adc))
                config_tdata <= {cfg_poffset_adc,
                                  !cfg_scan_enable_adc ? active_pinc :
                                  (scan_index + 1 < cfg_scan_count_adc) ?
                                  active_pinc + cfg_scan_step_adc :
                                  cfg_pinc_adc};
            adc_pipe[0] <= adc_centered;
            valid_pipe[0] <= i_sample_valid;
            for (k = 1; k <= MAX_DDS_LATENCY; k = k + 1) begin
                adc_pipe[k] <= adc_pipe[k-1];
                valid_pipe[k] <= valid_pipe[k-1];
            end
        end
    end

    // Results are produced only after the DDS pipeline warmup and are held
    // until software acknowledges them. Acknowledgement is also the scan
    // advance boundary, so no result can be associated with the wrong PINC.
    always_ff @(posedge clk_adc or negedge rst_n) begin
        if (!rst_n) begin
            i_acc <= '0;
            q_acc <= '0;
            o_i_sum <= '0;
            o_q_sum <= '0;
            o_sample_count <= '0;
            o_ready <= 1'b0;
            o_result_seq <= '0;
            o_result_pinc <= '0;
            result_toggle_adc <= 1'b0;
            result_ack_sync1 <= 1'b0;
            result_ack_sync2 <= 1'b0;
            result_ack_seen <= 1'b0;
            result_waiting_ack_adc <= 1'b0;
            measurement_enabled_adc <= 1'b0;
            sample_count <= '0;
            warmup_count <= '0;
            active_pinc <= '0;
            scan_index <= '0;
        end else begin
            o_ready <= 1'b0;
            result_ack_sync1 <= result_ack_toggle;
            result_ack_sync2 <= result_ack_sync1;
            if (cfg_apply_adc) begin
                i_acc <= '0;
                q_acc <= '0;
                sample_count <= '0;
                warmup_count <= DDS_LATENCY + 2;
                active_pinc <= cfg_pinc_adc;
                scan_index <= '0;
                result_waiting_ack_adc <= 1'b0;
                measurement_enabled_adc <= cfg_enable_adc;
            end else if (!measurement_enabled_adc) begin
                i_acc <= '0;
                q_acc <= '0;
                sample_count <= '0;
                warmup_count <= '0;
                result_waiting_ack_adc <= 1'b0;
                scan_index <= '0;
            end else if (result_waiting_ack_adc) begin
                if (result_ack_sync2 != result_ack_seen) begin
                    result_ack_seen <= result_ack_sync2;
                    result_waiting_ack_adc <= 1'b0;
                    i_acc <= '0;
                    q_acc <= '0;
                    sample_count <= '0;
                    if (!cfg_scan_enable_adc ||
                        (scan_index + 1 < cfg_scan_count_adc) ||
                        cfg_scan_repeat_adc) begin
                        if (cfg_scan_enable_adc) begin
                            active_pinc <=
                                (scan_index + 1 < cfg_scan_count_adc) ?
                                active_pinc + cfg_scan_step_adc : cfg_pinc_adc;
                            scan_index <= (scan_index + 1 < cfg_scan_count_adc) ?
                                          scan_index + 1'b1 : 16'd0;
                        end
                        warmup_count <= DDS_LATENCY + 2;
                    end else begin
                        measurement_enabled_adc <= 1'b0;
                        warmup_count <= '0;
                    end
                end
            end else if (warmup_count != 0) begin
                warmup_count <= warmup_count - 1'b1;
            end else if ((cfg_window_adc != 0) && lo_tvalid &&
                         valid_pipe[DDS_LATENCY]) begin
                if (sample_count == cfg_window_adc - 1'b1) begin
                    o_i_sum <= i_acc + i_product;
                    o_q_sum <= q_acc + q_product;
                    o_sample_count <= cfg_window_adc;
                    o_result_pinc <= active_pinc;
                    o_result_seq <= o_result_seq + 1'b1;
                    o_ready <= 1'b1;
                    result_toggle_adc <= ~result_toggle_adc;
                    result_waiting_ack_adc <= 1'b1;
                    i_acc <= '0;
                    q_acc <= '0;
                    sample_count <= '0;
                end else begin
                    i_acc <= i_acc + i_product;
                    q_acc <= q_acc + q_product;
                    sample_count <= sample_count + 1'b1;
                end
            end
        end
    end

endmodule
