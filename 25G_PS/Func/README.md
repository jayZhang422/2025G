# PS Func 使用入口

本目录只描述 `Identification_Processing_System/src` 下当前真实存在的自研代码。

## 先说结论

当前源码**没有统一 API**：不存在 `signal_api.h`、`signal_api.c`、
`signal_api_t` 或 `signal_capture()`。现在的唯一业务入口是：

```text
identification_main.c
  -> signal_separator_task()
    -> signal_separator_run()
      -> dma_utils / signal_processing / iq_demodulator / dds_control
```

因此，现阶段不要照 Doc 中的目标代码编写
`#include "signal_api.h"`，工程会找不到该头文件。Doc 描述的是下一步架构目标，
本目录描述的是现在能编译、能调用的接口。

## 从哪里开始看

1. 先读 `硬件接口与任务流程.md`，理解初始化和一次完整业务流程。
2. 再读 `函数索引.md`，按“结构体怎么创建、怎么传参”直接调用现有模块。
3. 需要改双分量 FFT 算法时，再读 `信号分析算法.md`。

## 当前代码分层

| 层次 | 当前文件 | 作用 |
| --- | --- | --- |
| FreeRTOS 入口 | `identification_main.c`、`signal_separator_task.c` | 创建任务、启动调度器 |
| 唯一业务流程 | `signal_separator_app.c` | `ARMED -> LOCKING -> RUNNING`，直接组合所有底层模块 |
| 驱动 | `User/src/dma_utils.c`、`iq_demodulator.c`、`dds_control.c`、`fifo_monitor.c`、`button_input.c` | 操作 DMA、AXI-Lite、BRAM 和 GPIO |
| 算法 | `User/src/signal_analysis.c`、`signal_processing.c` | FFT、候选搜索、双分量拟合和自测 |
| 配置 | `User/config/hardware_config.h`、`algorithm_config.h`、`signal_profiles.c/.h` | 硬件契约、算法默认值和题目参数 |
| 缓冲区 | `User/src/app_buffers.c` | DMA 缓冲区和 FFT 工作区的静态存储 |

当前 `signal_separator_app.c` 同时承担初始化、状态机、采集、算法组合和部分
错误处理，因而仍然偏重。配置分层、旧 `.txt` 主流程清理和日志前缀整理已经
完成；统一 API、统一错误码、通用测量、解调模式和 CLI 尚未实现。

## 文档边界

- `函数索引.md`：现有公开函数、结构体所有权和最小调用代码。
- `硬件接口与任务流程.md`：实际初始化顺序、缓存所有权和状态机。
- `信号分析算法.md`：4096 点双分量识别算法及其限制。
