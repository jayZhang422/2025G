# AD FIFO Monitor AXI 使用说明

## 1. 功能

`ad_fifo_monitor_axi` 是一个只观察、不反馈控制数据通路的 AXI4-Lite 外设。
它统计 ADC/FIFO/AXIS 入口侧事件，并由 PS 通过寄存器请求快照和读取结果。

这里的 “AXI” 指 AXI4-Lite 寄存器接口，不是 AXI-Stream 数据通道。监控点位于：

```text
ADC -> independent-clock FIFO -> H_top AXIS -> BD axis_data_fifo_0 -> AXI DMA
                                    ^ monitor observation point
```

因此 `axis_beat_count` 和 `frame_count` 表示数据已经被 BD 的 AXIS FIFO 接收，
不代表 AXI DMA 已完成，也不代表数据已经写入 DDR。DMA 完成必须继续读取 AXI DMA
状态或使用驱动接口判断。

## 2. 时钟与复位

- `adc_clk`：ADC/FIFO 写时钟，当前约 5.12006 MHz。
- `ad_fifo_monitor_axi_aclk`：AXI4-Lite 和 AXIS 观察时钟，当前为 PS FCLK0 100 MHz。
- `ad_fifo_monitor_axi_aresetn`：外设低有效复位。核心在两个时钟域内分别进行异步置位、同步释放。

ADC 域的多位计数器通过请求/应答 toggle 握手发布到 AXI 时钟域。PS 只能在
`STATUS.SNAPSHOT_VALID=1` 后读取一组快照。

## 3. 当前工程连接

- BD 实例：`fifo_monitor_axi_0`
- AXI4-Lite 基地址：`0x43C10000`
- 地址窗口：64 KiB
- 控制通路：PS `M_AXI_GP0 -> axi_interconnect_0/M03_AXI -> fifo_monitor_axi_0`
- 数据通路：所有 FIFO/AXIS 信号均为只读观察输入，不会产生 `tready`、FIFO 读写使能或复位反馈。

## 4. 寄存器表

所有寄存器为 32 位。除 `CONTROL` 外均只读；保留地址读回 0。

| 偏移 | 名称 | 属性 | 含义 |
| ---: | --- | --- | --- |
| `0x00` | `CONTROL` | W1P | bit0 请求快照；bit1 清除 sticky 状态 |
| `0x04` | `STATUS` | RO | 当前握手、sticky 和 FIFO 状态 |
| `0x08` | `VERSION` | RO | 当前值 `0x00010000` |
| `0x10/0x14` | `ADC_SAMPLE_LO/HI` | RO | ADC 有效样本数 |
| `0x18/0x1C` | `FIFO_WRITE_LO/HI` | RO | FIFO 实际允许写入数 |
| `0x20/0x24` | `BLOCKED_HIGH_LO/HI` | RO | 因 `prog_full/full` 阻止的写入数 |
| `0x28/0x2C` | `BLOCKED_RESET_LO/HI` | RO | 因 FIFO 写复位忙阻止的写入数 |
| `0x30/0x34` | `AXIS_BEAT_LO/HI` | RO | `tvalid && tready` beat 数 |
| `0x38/0x3C` | `FRAME_LO/HI` | RO | 成功握手的 `tlast` 数 |
| `0x40/0x44` | `AXIS_STALL_LO/HI` | RO | `tvalid && !tready` 周期数 |
| `0x48/0x4C` | `LAST_FRAME_TIME_LO/HI` | RO | 最近一帧末 beat 的 100 MHz 时间戳 |

`STATUS` 位定义：

| 位 | 名称 |
| ---: | --- |
| 0 | `SNAPSHOT_BUSY` |
| 1 | `SNAPSHOT_VALID` |
| 2 | `CLEAR_BUSY` |
| 3 | `WRITE_BLOCKED_STICKY` |
| 4 | `FIFO_FULL_STICKY` |
| 5 | `PROG_FULL_LIVE` |
| 6 | `FIFO_FULL_LIVE` |
| 7 | `WR_RESET_BUSY_LIVE` |
| 8 | `RD_RESET_BUSY_LIVE` |

## 5. PS 读取顺序

```c
#include "ad_fifo_monitor_axi.h"
#include "xparameters.h"

#define MON_BASE XPAR_FIFO_MONITOR_AXI_0_AD_FIFO_MONITOR_AXI_BASEADDR

static u64 monitor_read64(u32 low_offset, u32 high_offset)
{
    u32 low = AD_FIFO_MONITOR_AXI_mReadReg(MON_BASE, low_offset);
    u32 high = AD_FIFO_MONITOR_AXI_mReadReg(MON_BASE, high_offset);
    return ((u64)high << 32) | low;
}

AD_FIFO_MONITOR_AXI_mWriteReg(
    MON_BASE,
    AD_FIFO_MONITOR_AXI_CONTROL_OFFSET,
    AD_FIFO_MONITOR_AXI_CONTROL_SNAPSHOT_MASK);

while ((AD_FIFO_MONITOR_AXI_mReadReg(
            MON_BASE, AD_FIFO_MONITOR_AXI_STATUS_OFFSET) &
        AD_FIFO_MONITOR_AXI_STATUS_SNAPSHOT_VALID_MASK) == 0U) {
}

u64 frame_count = monitor_read64(
    AD_FIFO_MONITOR_AXI_FRAME_LO_OFFSET,
    AD_FIFO_MONITOR_AXI_FRAME_HI_OFFSET);
```

一次 snapshot 的 ADC 域计数和 AXI 域计数分别在各自域内保持一致并一起发布；由于
请求需要跨越 100 MHz/5.12 MHz 时钟域，两组计数并不是严格同一物理时刻。它适合
诊断吞吐、阻塞和长期计数差，不应当作为亚周期级时间对齐测量。

清除 sticky 状态：

```c
AD_FIFO_MONITOR_AXI_mWriteReg(
    MON_BASE,
    AD_FIFO_MONITOR_AXI_CONTROL_OFFSET,
    AD_FIFO_MONITOR_AXI_CONTROL_CLEAR_STICKY_MASK);
```

## 6. 工程刷新

修改 IP RTL 或 `component.xml` 后运行：

```text
vivado -mode batch -source 25G_PL/script/refresh_iq_ip_and_wrapper.tcl
```

该脚本只更新 IP、校验 BD、生成 BD 输出和 HDL wrapper；不启动仿真、综合、实现或 bitstream。

在 PS/Vitis 工程使用新的 `xparameters.h` 宏之前，还需要从当前 Vivado 设计重新导出
XSA 并更新 platform/BSP。仅刷新 BD 不会自动替换已有的 PS platform。
