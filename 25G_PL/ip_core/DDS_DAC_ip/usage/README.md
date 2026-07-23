# DDS/DAC 输出 IP 使用说明

## 1. 功能与边界

`DAC_DDS_Output` 是 125 MHz DAC 时钟域的双通道 DDS 输出核。它持续扫描外部 BRAM 的十个控制字，将新配置先收集到 shadow 寄存器，只有发现新的 `COMMIT_SEQ` 并完成一次完整扫描后才原子更新 A/B 运行配置。随后两个独立相位累加器访问外部正弦、三角或任意波形存储器，完成幅度缩放和 14 位饱和后驱动 AD9767 A/B。

本 IP 不包含 AXI 从接口、BRAM 控制器或波形 ROM。PS 到 BRAM 的 AXI-Lite 通路以及 ROM 由 Block Design/顶层完成。

## 2. 参数与端口

默认参数为 `PHASE_WIDTH=32`、`ADDR_WIDTH=12`、`DATA_WIDTH=14`。`clk` 为 125 MHz，`rst_n` 为低有效异步复位。`bram_addr/bram_dout/bram_en/bram_we` 形成只读 BRAM 端口；六组 `*_addr/*_data` 连接 A/B 的正弦、三角和任意波形存储器；`da_data_a/b` 与 `da_wrt_a/b` 连接 DAC 数据和写时钟。

波形编号：0 为正弦、1 为三角、2 为任意波形。波形 ROM 码值为以 8192 为中心的无符号 14 位数，IP 内部先去中点、再乘幅度、最后饱和到 0 至 16383。

## 3. BRAM 控制字

| 偏移 | 内容 |
| ---: | --- |
| `0x00`/`0x10` | A/B 波形选择。 |
| `0x04`/`0x14` | A/B 32 位相位步进。 |
| `0x08`/`0x18` | A/B 初始相位；B 相位增量模式时 `0x18` 解释为有符号增量。 |
| `0x0C`/`0x1C` | A/B 14 位幅度。 |
| `0x20` | bit0 `RUN`，bit1 `PHASE_RELOAD`，bit2 `B_PHASE_ADJUST`。 |
| `0x24` | `COMMIT_SEQ`，必须最后写入。 |

## 4. PS 写入和运行规则

1. 写入完整 A/B 波形、步进、相位和幅度，再写控制字。
2. 最后写入一个与上次不同的 `COMMIT_SEQ`，请求 PL 原子应用快照。
3. `RUN=0` 时 IP 清零相位累加器并将两个 DAC 输出固定在中点。
4. `RUN=1, PHASE_RELOAD=1` 时两个相位累加器装载绝对初相；常用于启动。
5. `RUN=1, B_PHASE_ADJUST=1` 时 A 正常推进，B 在正常步进基础上叠加带符号相位增量；常用于运行时微调。

PS 必须通过 `dds_control_commit` 和 `dds_control_adjust_b_phase` 访问本 IP，避免应用层遗漏“提交序号最后写”的协议要求。

## 5. 集成与验证

```text
PS GP0 -> AXI BRAM Controller -> blk_PS_TO_PL Port A
blk_PS_TO_PL Port B -> DAC_DDS_Output -> 外部波形 ROM -> AD9767
```

集成时确认 BRAM 读延迟、ROM 读延迟、DAC 写时钟极性和波形端口数据宽度。修改参数、ROM 或控制协议后，应重新封装 IP、更新 Block Design 输出、进行行为仿真和板级幅度/相位验证；不得手工修改生成的 `.xci`、wrapper 或缓存。
