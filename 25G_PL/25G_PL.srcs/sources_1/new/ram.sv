`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date: 2026/06/14 16:45:32
// Design Name: 
// Module Name: ram
// Project Name: 
// Target Devices: 
// Tool Versions: 
// Description: 
// 
// Dependencies: 
// 
// Revision:
// Revision 0.01 - File Created
// Additional Comments:
// 
//////////////////////////////////////////////////////////////////////////////////


module ram #(
    parameter DATA_WIDTH = 16,     
    parameter FFT_LENGTH = 4096    
)(
    input  logic                  clk,         
    // 65MHz
    input  logic                  rst,         // 低电平有效
    input  logic                  fifo_empty,
    input  logic [DATA_WIDTH-1:0] fifo_dout,   // 16bits (FWFT模式)
    output logic                  fifo_rd_en,  // ram_ready
  
    output logic [DATA_WIDTH-1:0] da_data      // 16bits
);
    // 分离写地址与读地址，彻底杜绝 DA 端由于 FIFO 变空而卡顿
    logic [11:0] wr_addr;
    // 写端专属地址 (0~4095)
    logic [11:0] rd_addr;            // 读端专属地址 (0~4095)
    
    logic        ping_pong_flag;
    // 0: BufferA写/BufferB读, 1: BufferA读/BufferB写
    logic        ping_pong_flag_d1;
    // 延迟一拍用于补偿BRAM读潜伏期

    // FWFT模式下，只要FIFO不空，我们就读取写入RAM
    assign fifo_rd_en = ~fifo_empty;

    //=========================================================
    // 写端地址逻辑：严格受 FIFO 有效数据流控制
    //=========================================================
    always_ff @(posedge clk or negedge rst) begin
        if (!rst) begin
            wr_addr        <= 12'd0;
            ping_pong_flag <= 1'b0;
        end else if (fifo_rd_en) begin
            wr_addr <= wr_addr + 1'b1;
            // 当写满 4096 个点时，自动跳转乒乓缓存
            if (wr_addr == 12'd4095) begin
                ping_pong_flag <= ~ping_pong_flag;
            end
        end
    end

    //=========================================================
    // 读端地址逻辑：彻底去除滞后判断，利用12位自然溢出归零，保持流水线连续
    //=========================================================
 
  always_ff @(posedge clk or negedge rst) begin
        if (!rst) begin
            rd_addr           <= 12'd0;
            ping_pong_flag_d1 <= 1'b0;
        end else if (fifo_rd_en) begin 
            ping_pong_flag_d1 <= ping_pong_flag;
            rd_addr <= rd_addr + 1'b1;
        end
    end

    //=========================================================
    // 读写通道的控制信号分配 (摒弃任何 ~addr 的危险操作)
    //=========================================================
    logic we_a, we_b;
    logic enb_a, enb_b;
    logic [11:0] addra_A, addrb_A;
    logic [11:0] addra_B, addrb_B;
    logic [DATA_WIDTH-1:0] dout_a, dout_b;

    // Buffer A 控制：
    // ping_pong_flag 为 0 时，写 A 读 B
    assign we_a  = (ping_pong_flag == 1'b0) ? fifo_rd_en : 1'b0;
    assign enb_a = (ping_pong_flag == 1'b1) ? 1'b1 : 1'b0;
    // 读 A 时读使能恒为 1，不卡顿
    assign addra_A = wr_addr;
    assign addrb_A = rd_addr;

    // Buffer B 控制：
    // ping_pong_flag 为 1 时，写 B 读 A
    assign we_b  = (ping_pong_flag == 1'b1) ? fifo_rd_en : 1'b0;
    assign enb_b = (ping_pong_flag == 1'b0) ? 1'b1 : 1'b0;
    // 读 B 时读使能恒为 1，不卡顿
    assign addra_B = wr_addr;
    assign addrb_B = rd_addr;

    // 数据输出选择（补偿1拍潜伏期）
    assign da_data = (ping_pong_flag_d1 == 1'b0) ? dout_b : dout_a;

    //=========================================================
    // BRAM IP 核实例化 (Simple Dual Port)
    //=========================================================
    blk_mem_gen_0 my_ram_bufferA (
        .clka  (clk),    
        .ena   (we_a),        // 仅在写入时开启 Port A 
        .wea   (we_a),        
        .addra (addra_A),  
        .dina  (fifo_dout),   
  
        .clkb  (clk),    
        .enb   (enb_a),       // 仅在读取时开启 Port B 
        .addrb (addrb_A),  
        .doutb (dout_a)  
    );

    blk_mem_gen_0 my_ram_bufferB (
        .clka  (clk),    
        .ena   (we_b),    
        .wea   (we_b),    
        .addra (addra_B),  
        .dina  (fifo_dout),    
        .clkb  (clk),    
        .enb   (enb_b),    
 
        .addrb (addrb_B),  
        .doutb (dout_b)  
    );

endmodule