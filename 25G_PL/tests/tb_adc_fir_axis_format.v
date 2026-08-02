`timescale 1ns / 1ps

// Standalone regression for the AD9248 raw-word format. The FIR stub below
// keeps this test independent of generated Vivado IP output products.
module tb_adc_fir_axis_format;
    reg  [15:0] s_axis_tdata;
    wire        s_axis_tready;

    adc_fir_axis dut (
        .aclk          (1'b0),
        .aresetn       (1'b0),
        .s_axis_tdata  (s_axis_tdata),
        .s_axis_tvalid (1'b0),
        .s_axis_tready (s_axis_tready),
        .m_axis_tdata  (),
        .m_axis_tvalid (),
        .m_axis_tready (1'b1),
        .m_axis_tlast  ()
    );

    task check_code;
        input        [13:0] raw_code;
        input signed [15:0] expected;
        begin
            s_axis_tdata = {raw_code, 2'b00};
            #1;
            if ($signed(dut.fir_s_tdata) !== expected)
                $fatal(1, "raw=%h converted to %0d, expected %0d",
                       raw_code, $signed(dut.fir_s_tdata), expected);
        end
    endtask

    initial begin
        check_code(14'h0000,  16'sd0);
        check_code(14'h1fff,  16'sd8191);
        check_code(14'h2000, -16'sd8192);
        check_code(14'h3fff, -16'sd1);
        $display("PASS: AD9248 two's-complement samples map to signed16 codes");
        $finish;
    end
endmodule

module fir_compiler_0 (
    input  wire        aresetn,
    input  wire        aclk,
    input  wire        s_axis_data_tvalid,
    output wire        s_axis_data_tready,
    input  wire [15:0] s_axis_data_tdata,
    output wire        m_axis_data_tvalid,
    input  wire        m_axis_data_tready,
    output wire [39:0] m_axis_data_tdata
);
    assign s_axis_data_tready = 1'b1;
    assign m_axis_data_tvalid = 1'b0;
    assign m_axis_data_tdata  = 40'd0;
endmodule
