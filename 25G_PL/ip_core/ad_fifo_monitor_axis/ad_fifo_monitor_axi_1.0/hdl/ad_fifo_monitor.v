`timescale 1ns / 1ps

// Passive ADC/FIFO/AXIS monitor. ADC-domain counters are transferred to the
// AXI clock domain with a bundled-data toggle handshake. The data-path inputs
// are observation-only and never participate in FIFO or AXIS control.
module ad_fifo_monitor (
    input  wire        rst_n,
    input  wire        adc_clk,
    input  wire        axis_clk,

    input  wire        sample_valid,
    input  wire        fifo_write,
    input  wire        fifo_prog_full,
    input  wire        fifo_full,
    input  wire        fifo_wr_rst_busy,
    input  wire        fifo_rd_rst_busy,

    input  wire        axis_tvalid,
    input  wire        axis_tready,
    input  wire        axis_tlast,

    input  wire        snapshot,
    input  wire        clear_sticky,
    output wire        snapshot_busy,
    output reg         snapshot_valid,
    output wire        clear_busy,

    output reg  [63:0] adc_sample_count,
    output reg  [63:0] fifo_write_count,
    output reg  [63:0] blocked_high_watermark_count,
    output reg  [63:0] blocked_reset_count,
    output reg  [63:0] axis_beat_count,
    output reg  [63:0] frame_count,
    output reg  [63:0] axis_stall_cycle_count,
    output reg  [63:0] last_frame_timestamp,

    output wire        write_blocked_sticky,
    output wire        fifo_full_sticky,
    output wire        status_prog_full,
    output wire        status_fifo_full,
    output wire        status_wr_rst_busy,
    output wire        status_rd_rst_busy
);

    (* ASYNC_REG = "TRUE" *) reg [1:0] adc_reset_sync;
    (* ASYNC_REG = "TRUE" *) reg [1:0] axis_reset_sync;
    wire adc_reset_n  = adc_reset_sync[1];
    wire axis_reset_n = axis_reset_sync[1];

    always @(posedge adc_clk or negedge rst_n)
        if (!rst_n) adc_reset_sync <= 2'b00;
        else        adc_reset_sync <= {adc_reset_sync[0], 1'b1};

    always @(posedge axis_clk or negedge rst_n)
        if (!rst_n) axis_reset_sync <= 2'b00;
        else        axis_reset_sync <= {axis_reset_sync[0], 1'b1};

    reg snapshot_req_toggle, snapshot_ack_toggle, snapshot_ack_seen;
    reg clear_req_toggle, clear_ack_toggle;
    (* ASYNC_REG = "TRUE" *) reg [1:0] snapshot_req_sync, clear_req_sync;
    (* ASYNC_REG = "TRUE" *) reg [1:0] snapshot_ack_sync, clear_ack_sync;

    assign snapshot_busy = (snapshot_req_toggle != snapshot_ack_sync[1]) ||
                           (snapshot_ack_sync[1] != snapshot_ack_seen);
    assign clear_busy    = clear_req_toggle != clear_ack_sync[1];

    always @(posedge adc_clk or negedge adc_reset_n) begin
        if (!adc_reset_n) begin
            snapshot_req_sync <= 2'b00;
            clear_req_sync    <= 2'b00;
        end else begin
            snapshot_req_sync <= {snapshot_req_sync[0], snapshot_req_toggle};
            clear_req_sync    <= {clear_req_sync[0], clear_req_toggle};
        end
    end

    always @(posedge axis_clk or negedge axis_reset_n) begin
        if (!axis_reset_n) begin
            snapshot_ack_sync <= 2'b00;
            clear_ack_sync    <= 2'b00;
        end else begin
            snapshot_ack_sync <= {snapshot_ack_sync[0], snapshot_ack_toggle};
            clear_ack_sync    <= {clear_ack_sync[0], clear_ack_toggle};
        end
    end

    reg [63:0] adc_sample_live, fifo_write_live;
    reg [63:0] blocked_high_watermark_live, blocked_reset_live;
    reg        write_blocked_sticky_adc, fifo_full_sticky_adc;
    reg [63:0] adc_sample_hold, fifo_write_hold;
    reg [63:0] blocked_high_watermark_hold, blocked_reset_hold;

    always @(posedge adc_clk or negedge adc_reset_n) begin
        if (!adc_reset_n) begin
            adc_sample_live             <= 64'd0;
            fifo_write_live             <= 64'd0;
            blocked_high_watermark_live <= 64'd0;
            blocked_reset_live          <= 64'd0;
            write_blocked_sticky_adc    <= 1'b0;
            fifo_full_sticky_adc        <= 1'b0;
            adc_sample_hold             <= 64'd0;
            fifo_write_hold             <= 64'd0;
            blocked_high_watermark_hold <= 64'd0;
            blocked_reset_hold          <= 64'd0;
            snapshot_ack_toggle         <= 1'b0;
            clear_ack_toggle            <= 1'b0;
        end else begin
            if (sample_valid) begin
                adc_sample_live <= adc_sample_live + 1'b1;
                if (fifo_write)
                    fifo_write_live <= fifo_write_live + 1'b1;
                else if (fifo_wr_rst_busy)
                    blocked_reset_live <= blocked_reset_live + 1'b1;
                else if (fifo_prog_full || fifo_full) begin
                    blocked_high_watermark_live <= blocked_high_watermark_live + 1'b1;
                    write_blocked_sticky_adc    <= 1'b1;
                end
            end

            if (clear_req_sync[1] != clear_ack_toggle) begin
                write_blocked_sticky_adc <= 1'b0;
                fifo_full_sticky_adc     <= 1'b0;
                clear_ack_toggle         <= clear_req_sync[1];
            end
            if (sample_valid && !fifo_write && !fifo_wr_rst_busy &&
                (fifo_prog_full || fifo_full))
                write_blocked_sticky_adc <= 1'b1;
            if (fifo_full && !fifo_wr_rst_busy)
                fifo_full_sticky_adc <= 1'b1;

            if (snapshot_req_sync[1] != snapshot_ack_toggle) begin
                adc_sample_hold             <= adc_sample_live;
                fifo_write_hold             <= fifo_write_live;
                blocked_high_watermark_hold <= blocked_high_watermark_live;
                blocked_reset_hold          <= blocked_reset_live;
                snapshot_ack_toggle         <= snapshot_req_sync[1];
            end
        end
    end

    reg [63:0] axis_beat_live, frame_live, axis_stall_live;
    reg [63:0] axis_time_live, last_frame_time_live;
    wire axis_transfer = axis_tvalid && axis_tready;

    always @(posedge axis_clk or negedge axis_reset_n) begin
        if (!axis_reset_n) begin
            axis_beat_live       <= 64'd0;
            frame_live           <= 64'd0;
            axis_stall_live      <= 64'd0;
            axis_time_live       <= 64'd0;
            last_frame_time_live <= 64'd0;
        end else begin
            axis_time_live <= axis_time_live + 1'b1;
            if (axis_transfer)
                axis_beat_live <= axis_beat_live + 1'b1;
            if (axis_tvalid && !axis_tready)
                axis_stall_live <= axis_stall_live + 1'b1;
            if (axis_transfer && axis_tlast) begin
                frame_live           <= frame_live + 1'b1;
                last_frame_time_live <= axis_time_live;
            end
        end
    end

    (* ASYNC_REG = "TRUE" *) reg [1:0] blocked_sticky_sync, full_sticky_sync;
    (* ASYNC_REG = "TRUE" *) reg [1:0] prog_full_sync, fifo_full_sync;
    (* ASYNC_REG = "TRUE" *) reg [1:0] wr_busy_sync, rd_busy_sync;

    assign write_blocked_sticky = blocked_sticky_sync[1];
    assign fifo_full_sticky     = full_sticky_sync[1];
    assign status_prog_full     = prog_full_sync[1];
    assign status_fifo_full     = fifo_full_sync[1];
    assign status_wr_rst_busy   = wr_busy_sync[1];
    assign status_rd_rst_busy   = rd_busy_sync[1];

    always @(posedge axis_clk or negedge axis_reset_n) begin
        if (!axis_reset_n) begin
            blocked_sticky_sync <= 2'b00;
            full_sticky_sync    <= 2'b00;
            prog_full_sync      <= 2'b00;
            fifo_full_sync      <= 2'b00;
            wr_busy_sync        <= 2'b00;
            rd_busy_sync        <= 2'b00;
        end else begin
            blocked_sticky_sync <= {blocked_sticky_sync[0], write_blocked_sticky_adc};
            full_sticky_sync    <= {full_sticky_sync[0], fifo_full_sticky_adc};
            prog_full_sync      <= {prog_full_sync[0], fifo_prog_full};
            fifo_full_sync      <= {fifo_full_sync[0], fifo_full};
            wr_busy_sync        <= {wr_busy_sync[0], fifo_wr_rst_busy};
            rd_busy_sync        <= {rd_busy_sync[0], fifo_rd_rst_busy};
        end
    end

    reg snapshot_d, clear_sticky_d;

    always @(posedge axis_clk or negedge axis_reset_n) begin
        if (!axis_reset_n) begin
            snapshot_d                  <= 1'b0;
            clear_sticky_d               <= 1'b0;
            snapshot_req_toggle          <= 1'b0;
            clear_req_toggle             <= 1'b0;
            snapshot_ack_seen            <= 1'b0;
            snapshot_valid               <= 1'b0;
            adc_sample_count             <= 64'd0;
            fifo_write_count             <= 64'd0;
            blocked_high_watermark_count <= 64'd0;
            blocked_reset_count          <= 64'd0;
            axis_beat_count              <= 64'd0;
            frame_count                  <= 64'd0;
            axis_stall_cycle_count       <= 64'd0;
            last_frame_timestamp         <= 64'd0;
        end else begin
            snapshot_d     <= snapshot;
            clear_sticky_d <= clear_sticky;

            if (snapshot && !snapshot_d && !snapshot_busy) begin
                snapshot_req_toggle    <= ~snapshot_req_toggle;
                snapshot_valid         <= 1'b0;
                axis_beat_count        <= axis_beat_live;
                frame_count            <= frame_live;
                axis_stall_cycle_count <= axis_stall_live;
                last_frame_timestamp   <= last_frame_time_live;
            end

            if (clear_sticky && !clear_sticky_d && !clear_busy)
                clear_req_toggle <= ~clear_req_toggle;

            if (snapshot_ack_sync[1] != snapshot_ack_seen) begin
                snapshot_ack_seen            <= snapshot_ack_sync[1];
                adc_sample_count             <= adc_sample_hold;
                fifo_write_count             <= fifo_write_hold;
                blocked_high_watermark_count <= blocked_high_watermark_hold;
                blocked_reset_count          <= blocked_reset_hold;
                snapshot_valid               <= 1'b1;
            end
        end
    end

endmodule
