# PS Func 使用入口

本目录描述 `Identification_Processing_System/src` 下当前真实存在、已纳入
Vitis `-O2` 构建的 PS 自研代码。若文档示例与源码不一致，以
`User/include/signal_api.h` 的声明为准。

## 从哪里开始

新业务代码只需要先包含统一入口：

```c
#include "User/include/signal_api.h"
```

现有完整调用链是：

```text
identification_main.c
  -> signal_separator_task()
    -> signal_separator_run()             现有比赛模式
      -> signal_api.h / signal_api.c       统一公共入口
        -> dma_utils / fifo_monitor / iq_demodulator / dds_control
        -> signal_analysis / app_buffers
```

阅读顺序建议：

1. `函数索引.md`：直接查初始化、profile、采集、分析、IQ、DDS、状态和传参示例。
2. `硬件接口与任务流程.md`：理解 PS/PL 边界、Cache、首次帧对齐和状态机。
3. `信号分析算法.md`：只在修改现有双分量正弦/三角识别器时阅读。

## 当前分层

| 层次 | 文件 | 作用 |
| --- | --- | --- |
| FreeRTOS 入口 | `identification_main.c`、`signal_separator_task.c` | 创建任务、启动调度器 |
| 比赛模式 | `signal_separator_app.c` | 描述 `ARMED -> LOCKING -> RUNNING`，不直接操作寄存器或 Cache |
| 统一 API | `User/include/signal_api.h`、`User/src/signal_api.c` | 组合初始化、对齐、采集、识别、IQ、DDS、按键、状态和恢复 |
| 公共类型/错误 | `signal_types.h`、`signal_errors.h` | 帧、结果、质量标志和第一失败点 |
| 驱动 | `dma_utils`、`fifo_monitor`、`iq_demodulator`、`dds_control`、`button_input` | 操作现有 PS/PL 接口 |
| 算法 | `signal_analysis.c`、`signal_processing.c` | 4096 点 FFT、双分量搜索和拟合 |
| 配置 | `hardware_config.h`、`algorithm_config.h`、`signal_profiles.c/.h` | 硬件契约、算法固定值、题目 profile |
| 静态缓冲 | `app_buffers.c/.h` | DMA 缓冲和 FFT 工作区；不占任务栈 |

## 最小使用骨架

```c
signal_api_t api;
signal_frame_t frame;
signal_analysis_result_t result;
const signal_profile_t *profile = signal_profile_default();
signal_error_t error;

error = signal_api_init(&api, profile);
if (error != SIGNAL_OK) {
    xil_printf("init: %s\r\n", signal_error_string(error));
    return;
}

/* 开始一次新的快照测量前执行一次，不要每个分析帧都执行。 */
if (signal_align_capture(&api) != SIGNAL_OK) {
    signal_recover(&api);
    return;
}

if (signal_capture(&api, &frame) == SIGNAL_OK &&
    signal_identify_components(&api, &frame, &result) == SIGNAL_OK) {
    /* result.channel_a / channel_b / normalized_residual 可用。 */
}
```

`signal_api_t`、`signal_frame_t` 和结果结构都由调用者声明，不使用动态内存。
4096 点 DMA 与 FFT 大数组仍由 `app_buffers.c` 静态持有，不能复制到任务栈。

## 本次整理的边界

已完成：

- 单一 `signal_api.h` 公共入口和 `signal_api_t` 运行对象；
- 统一 `signal_error_t`；
- KEY1 后首次正式采集前显式 reset/丢弃至 `TLAST`；
- 每帧 monitor 前后快照、实际 DMA 长度和采集/错误/对齐计数；
- 旧题锁定阈值、超时、允许波形、DDS 幅度和相位参数进入 profile；
- 现有比赛模式改用统一 API，原 FFT/IQ/DDS 功能保持不变。

没有实现：通用 DC/RMS/Vpp、AM/ASK/FSK/FM、CLI、连续 DMA、双缓冲、
校准或任何新 PL 逻辑。`general_measure`、`weak_signal`、`am`、`ask`、
`2fsk`、`fm` 仍是零参数占位 profile，不能直接传给 `signal_api_init()`。
