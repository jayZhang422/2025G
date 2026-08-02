module ad_fifo_wrapper(
    input  wire        rst_n,        // 系统异步复位 (低电平有效)

    // ===== 采样与写入时钟 (外部 PLL 输入) =====
    input  wire        clk_phase,    // 带相移的采样/写时钟 (5.12MHz 181.8deg)

    // ===== AD9248 单通道物理数据接口 =====
    input  wire [13:0] adc_din,      // AD9248 14-bit 复用数据总线

    // ===== FIFO 读端口 (对接外部 H_top / 逻辑) =====
    input  wire        rd_clk,       // 读侧时钟 (100MHz PS FCLK)
    input  wire        rd_en,        // 读使能
    output wire [15:0] dout,         // 16位高位对齐/低位补零输出数据
    output wire        empty,        // FIFO 空标志

    // ADC-clock-domain observation path for the IQ demodulator. These are
    // the same registered sample and validity flag that feed the FIFO write
    // port; they do not add a second capture path.
    output wire [11:0] adc_raw,
    output wire        sample_valid,

    // Read-only monitor taps; none of them feed back into the data path.
    output wire        mon_fifo_write,
    output wire        mon_prog_full,
    output wire        mon_fifo_full,
    output wire        mon_wr_rst_busy,
    output wire        mon_rd_rst_busy
    //需在顶层写一个0度时钟给ADC引脚约束使用
);


    wire [13:0] ad_dout;
    wire        ad_valid;

    // 1. 例化采样寄存器（保留原模块名以维持工程接口）
    ad9226 u_ad9226 (
        .rst_ad       (rst_n),
        .din          (adc_din),
        .clk_phase    (clk_phase),
        .dout         (ad_dout),
        .ad_out_valid (ad_valid)
    );

    // 2. 原封不动例化 fifo 模块
    fifo u_fifo (
        .rst          (rst_n),
        .wr_clk       (clk_phase),   // 使用相移时钟作为 FIFO 写时钟
        .wr_en        (ad_valid),    // 采样数据稳定有效时开启写入
        .din          (ad_dout),     // 14-bit 寄存后的原始数据
        .rd_clk       (rd_clk),      // 外部 100MHz 读时钟
        .rd_en        (rd_en),       // 外部读使能
        .dout         (dout),        // 16-bit 输出
        .empty        (empty),       // 保留现有 prog_empty 门控语义
        .mon_write_en (mon_fifo_write),
        .mon_prog_full(mon_prog_full),
        .mon_fifo_full(mon_fifo_full),
        .mon_wr_rst_busy(mon_wr_rst_busy),
        .mon_rd_rst_busy(mon_rd_rst_busy)
    );

    // 保持遗留 IQ/DDC 的 12-bit offset-binary 接口不变。AD9248 模块将
    // DFS 上拉为补码输出，因此翻转符号位并丢弃两个 LSB 后再送观察支路。
    assign adc_raw      = {~ad_dout[13], ad_dout[12:2]};
    assign sample_valid = ad_valid;




endmodule
