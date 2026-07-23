# PS Func 文档索引

本目录记录 `Identification_Processing_System/src` 下的自研应用逻辑，不包括 CMSIS、FreeRTOS、Vitis 自动生成 BSP 和平台文件。

| 文档 | 内容 |
| --- | --- |
| `信号分析算法.md` | DMA 帧经 FFT、候选频率、波形拟合与锁定确认的算法。 |
| `硬件接口与任务流程.md` | FreeRTOS 任务、DMA、DDS BRAM、按键和 PL IQ 解调器的调用顺序。 |
| `函数索引.md` | 自研公开接口的职责、主要参数和最小使用方式。 |

应用首先使用 DMA+FFT 识别输入，再一次性提交 DDS 配置；PL IQ 解调器是对已知频点进行幅相测量的补充路径，不能替代未知频率搜索。
