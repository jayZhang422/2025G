`timescale 1ns / 1ps

// Converts the raw AD9226 stream to signed samples, applies the fixed
// decimating FIR, and restores the 16-bit/4096-sample DMA frame contract.
module adc_fir_axis (
    input  wire        aclk,
    input  wire        aresetn,

    input  wire [15:0] s_axis_tdata,
    input  wire        s_axis_tvalid,
    output wire        s_axis_tready,

    output wire [15:0] m_axis_tdata,
    output wire        m_axis_tvalid,
    input  wire        m_axis_tready,
    output wire        m_axis_tlast
);

    wire signed [12:0] adc_centered;
    wire        [15:0] fir_s_tdata;
    wire               fir_m_tvalid;
    wire        [39:0] fir_m_tdata;
    reg         [11:0] output_count;

    assign adc_centered = 13'sd2048 -
                          $signed({1'b0, s_axis_tdata[15:4]});
    assign fir_s_tdata = {{3{adc_centered[12]}}, adc_centered};

    fir_compiler_0 u_fir (
        .aresetn            (aresetn),
        .aclk               (aclk),
        .s_axis_data_tvalid (s_axis_tvalid),
        .s_axis_data_tready (s_axis_tready),
        .s_axis_data_tdata  (fir_s_tdata),
        .m_axis_data_tvalid (fir_m_tvalid),
        .m_axis_data_tready (m_axis_tready),
        .m_axis_data_tdata  (fir_m_tdata)
    );

    // The imported integer coefficients sum to 2^17. Convert the 34 valid
    // full-precision result bits back to signed integer ADC codes.
    function [15:0] q17_to_s16;
        input signed [33:0] value;
        reg signed [34:0] extended_value;
        reg signed [34:0] rounded_value;
        begin
            extended_value = {value[33], value};
            if (extended_value >= 0)
                rounded_value = (extended_value + 35'sd65536) >>> 17;
            else
                rounded_value = -(((-extended_value) + 35'sd65536) >>> 17);

            if (rounded_value > 35'sd32767)
                q17_to_s16 = 16'h7fff;
            else if (rounded_value < -35'sd32768)
                q17_to_s16 = 16'h8000;
            else
                q17_to_s16 = rounded_value[15:0];
        end
    endfunction

    assign m_axis_tdata  = q17_to_s16($signed(fir_m_tdata[33:0]));
    assign m_axis_tvalid = fir_m_tvalid;
    assign m_axis_tlast  = fir_m_tvalid && (output_count == 12'd4095);

    always @(posedge aclk) begin
        if (!aresetn)
            output_count <= 12'd0;
        else if (m_axis_tvalid && m_axis_tready)
            output_count <= (output_count == 12'd4095) ?
                            12'd0 : output_count + 1'b1;
    end

endmodule
