# PS Func 文档索引

本目录记录 `Identification_Processing_System/src` 下的自研应用逻辑，不包括 CMSIS、FreeRTOS、Vitis 自动生成 BSP 和平台文件。

| 文档 | 内容 |
| --- | --- |
| `信号分析算法.md` | DMA 帧经 FFT、候选频率、波形拟合与锁定确认的算法。 |
| `硬件接口与任务流程.md` | FreeRTOS 任务、DMA、DDS BRAM、按键和 PL IQ 解调器的调用顺序。 |
| `函数索引.md` | 自研公开接口的职责、主要参数和最小使用方式。 |

当前入口链为 `identification_main.c -> signal_separator_task.c ->
signal_separator_app.c`。应用首先使用 DMA+FFT 识别输入，再一次性提交
DDS 配置；PL IQ 解调器是对已知频点进行幅相测量的补充路径，不能替代
未知频率搜索。

配置分为三类：`User/config/hardware_config.h` 保存不可由题目模式修改的
PS/PL 契约，`algorithm_config.h` 保存现有算法默认值，
`signal_profiles.c/.h` 保存题目级参数。当前应用只选择
`PROFILE_OLD_SEPARATOR`；其他 profile 尚未填入题目参数，也未实现对应算法。

应用源码只以正常 `.c/.h` 编译单元参与构建，不包含 `.txt` 或其他 `.c`
源文件。串口诊断沿用现有打印数量和字段，并按 `[APP]`、`[ADC]`、
`[DMA]`、`[FIFO]`、`[FFT]`、`[IQ]`、`[DDS]` 标明故障域。
