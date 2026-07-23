`timescale 1ns / 1ps
// Active G2025 top.  The legacy top/H_top hierarchy remains unmodified.
module g2025_top_v3(
 input wire i_clk_50m,input wire i_rst,input wire [2:0] pl_key_i,input wire [11:0] i_ad_data,output wire o_ad_clk,output wire [13:0] o_da_data,output wire o_da_wrt,output wire o_da_clk,output wire [13:0] o_da_data_b,output wire o_da_wrt_b,output wire o_da_clk_b,
 inout wire [14:0] DDR_addr,inout wire [2:0] DDR_ba,inout wire DDR_cas_n,inout wire DDR_ck_n,inout wire DDR_ck_p,inout wire DDR_cke,inout wire DDR_cs_n,inout wire [3:0] DDR_dm,inout wire [31:0] DDR_dq,inout wire [3:0] DDR_dqs_n,inout wire [3:0] DDR_dqs_p,inout wire DDR_odt,inout wire DDR_ras_n,inout wire DDR_reset_n,inout wire DDR_we_n,inout wire FIXED_IO_ddr_vrn,inout wire FIXED_IO_ddr_vrp,inout wire [53:0] FIXED_IO_mio,inout wire FIXED_IO_ps_clk,inout wire FIXED_IO_ps_porb,inout wire FIXED_IO_ps_srstb);
 wire clk_dac,fclk,ad_clk,ad_phase,ad_valid,fifo_empty,fifo_rd_en,adc_tvalid,adc_tready,adc_tlast;
 wire [11:0] ad_data; wire [15:0] fifo_data,adc_tdata; wire [31:0] ctrl_bram_addr,ctrl_bram_dout,wave_bram_addr,wave_bram_dout; wire ctrl_bram_en,wave_bram_en; wire [3:0] ctrl_bram_we,wave_bram_we; reg [11:0] tlast_count; wire dac_clk_a,dac_clk_b,dac_wrt_a,dac_wrt_b;
 assign o_ad_clk=ad_clk;
 ad9226 u_adc_clocking(.rst_ad(i_rst),.clk(i_clk_50m),.din(i_ad_data),.dout(ad_data),.clk_sys_out(),.clk_ad(ad_clk),.clk_ad_deg(ad_phase),.ad_out_valid(ad_valid));
 fifo u_adc_fifo(.rst(i_rst&ad_valid),.wr_clk(ad_phase),.wr_en(ad_valid),.din(ad_data),.rd_clk(fclk),.rd_en(fifo_rd_en),.dout(fifo_data),.empty(fifo_empty));
 assign fifo_rd_en=~fifo_empty&adc_tready; assign adc_tdata=fifo_data; assign adc_tvalid=~fifo_empty;
 always @(posedge fclk or negedge i_rst) begin if(!i_rst)tlast_count<=12'd0; else if(adc_tvalid&&adc_tready)tlast_count<=(tlast_count==12'd4095)?12'd0:tlast_count+1'b1; end
 assign adc_tlast=(tlast_count==12'd4095)&&adc_tvalid;
 ODDR #(.DDR_CLK_EDGE("OPPOSITE_EDGE")) u_clk_a(.C(clk_dac),.CE(1'b1),.D1(1'b1),.D2(1'b0),.Q(dac_clk_a),.R(~i_rst),.S(1'b0));
 ODDR #(.DDR_CLK_EDGE("OPPOSITE_EDGE")) u_clk_b(.C(clk_dac),.CE(1'b1),.D1(1'b1),.D2(1'b0),.Q(dac_clk_b),.R(~i_rst),.S(1'b0));
 ODDR #(.DDR_CLK_EDGE("OPPOSITE_EDGE")) u_wrt_a(.C(clk_dac),.CE(1'b1),.D1(1'b1),.D2(1'b0),.Q(dac_wrt_a),.R(~i_rst),.S(1'b0));
 ODDR #(.DDR_CLK_EDGE("OPPOSITE_EDGE")) u_wrt_b(.C(clk_dac),.CE(1'b1),.D1(1'b1),.D2(1'b0),.Q(dac_wrt_b),.R(~i_rst),.S(1'b0));
 assign o_da_clk=dac_clk_a; assign o_da_wrt=dac_wrt_a; assign o_da_clk_b=dac_clk_b; assign o_da_wrt_b=dac_wrt_b;
 g2025_dac_adapter_v3 u_dac(.clk(clk_dac),.rst_n(i_rst),.ctrl_bram_addr(ctrl_bram_addr),.ctrl_bram_dout(ctrl_bram_dout),.ctrl_bram_en(ctrl_bram_en),.ctrl_bram_we(ctrl_bram_we),.wave_bram_addr(wave_bram_addr),.wave_bram_dout(wave_bram_dout),.wave_bram_en(wave_bram_en),.wave_bram_we(wave_bram_we),.da_data_a(o_da_data),.da_data_b(o_da_data_b),.da_wrt_a(),.da_wrt_b());
 system_wrapper u_system(.ADC_STREAM_IN_tdata(adc_tdata),.ADC_STREAM_IN_tlast(adc_tlast),.ADC_STREAM_IN_tready(adc_tready),.ADC_STREAM_IN_tvalid(adc_tvalid),.BRAM_DATA_addr(ctrl_bram_addr),.BRAM_DATA_clk(clk_dac),.BRAM_DATA_din(32'd0),.BRAM_DATA_dout(ctrl_bram_dout),.BRAM_DATA_en(ctrl_bram_en),.BRAM_DATA_rst(~i_rst),.BRAM_DATA_we(ctrl_bram_we),.WAVE_RAM_addr(wave_bram_addr),.WAVE_RAM_clk(clk_dac),.WAVE_RAM_din(32'd0),.WAVE_RAM_dout(wave_bram_dout),.WAVE_RAM_en(wave_bram_en),.WAVE_RAM_rst(~i_rst),.WAVE_RAM_we(wave_bram_we),.DDR_addr(DDR_addr),.DDR_ba(DDR_ba),.DDR_cas_n(DDR_cas_n),.DDR_ck_n(DDR_ck_n),.DDR_ck_p(DDR_ck_p),.DDR_cke(DDR_cke),.DDR_cs_n(DDR_cs_n),.DDR_dm(DDR_dm),.DDR_dq(DDR_dq),.DDR_dqs_n(DDR_dqs_n),.DDR_dqs_p(DDR_dqs_p),.DDR_odt(DDR_odt),.DDR_ras_n(DDR_ras_n),.DDR_reset_n(DDR_reset_n),.DDR_we_n(DDR_we_n),.FCLK_CLK0_0(fclk),.FIXED_IO_ddr_vrn(FIXED_IO_ddr_vrn),.FIXED_IO_ddr_vrp(FIXED_IO_ddr_vrp),.FIXED_IO_mio(FIXED_IO_mio),.FIXED_IO_ps_clk(FIXED_IO_ps_clk),.FIXED_IO_ps_porb(FIXED_IO_ps_porb),.FIXED_IO_ps_srstb(FIXED_IO_ps_srstb),.clk_dac(clk_dac),.pl_key_i(pl_key_i));
endmodule
