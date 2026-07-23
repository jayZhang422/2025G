# 2025G Vitis Architecture

## Status

The domain is `freertos10_xilinx`; active `identification_main.c` is still FreeRTOS Hello World. No 2025G workflow is implemented yet.

## Target modules

```text
app_state
|- board_buttons / display_ui
|- dma_capture
|- dds_control / wave_ram
|- dac_vpp_calibration / transfer_function_model
|- rlc_learning
`- response_waveform_generator
```

Hardware access stays in drivers. Algorithms receive typed data and never raw PL addresses.

Data products:

- `dac_vpp_calibration_t`: DAC amplitude and ADC gain/offset dac_vpp_calibration.
- `complex_response_t`: learned frequency, gain and phase points.
- `rlc_model_t`: filter class and optional fitted coefficients.
- `wave_table_t`: 4096 unsigned 14-bit samples plus replay frequency.

Learning must finish in 120 seconds; basic and inference output must start within 5 seconds. Frequency estimation and replay use the common ADC/DAC reference ratio.


## Handoff status - 2026-07-22

Use repository-root CODEX_START.md as the only new-session entry. Both packaged PL IPs passed isolated behavioral regression, but the active XSA/platform contract has not changed. Do not implement PS access to arbitrary-wave RAM until PL integration, synthesis, XSA export, and BSP regeneration expose verified XPAR_* identifiers.

## 2026-07-23 执行架构：软件分阶段落地

本节对应当前题目方案。当前工程仍是 FreeRTOS Hello World；以下模块按验证门逐步加入，未生成新 XSA/BSP 前不宣称波表 PS 访问已完成。

### 服务与数据流

- transfer_function_model：保存已知 RLC 模型参数，计算 H(j2πf) 并与扫频结果比较。
- dac_vpp_calibration：保存 DAC 幅度码到实际 Vpp 的离线标定关系；基本 3/4 按 Vin,pp = Vout,target,pp / |H(j2πf)| 开环计算 DDS 幅度。
- open_loop_output_planner：执行基本 1–4 的参数检查、一次性 DDS 设置和结果输出；运行期间不启用 ADC/PID 反馈。纯软件开环计划器已实现并通过主机自检。
- coherent_transfer_measurement：用 I=(2/N)Σy[n]cos(2πfn/fs)、Q=-(2/N)Σy[n]sin(2πfn/fs) 得到 Y=I+jQ，再除以输入参考 X 得到复数 H。
- rlc_learning / rlc_filter_classifier：管理未知 RLC 扫频、复响应缓存及低通/高通/带通/带阻分类。
- fundamental_frequency_estimator / response_waveform_generator：捕获未知输入、估计基频、用学习到的响应系数生成 4096 点重放波表。
- wave_ram：仅在新 XSA 生成并确认对应 XPAR 后实现；PS 写表时 DDS 停止，完成后一次性提交并启动，避免播放中更新半表。

### 学习与重放状态

BOOT → MENU → BASIC
             ├─ LEARN: DDS 扫频 + ADC 同步解调 + 分类/缓存
             └─ INFER: ADC 只采未知输入 → PS 生成表 → PL 确定性循环播放

学习阶段允许未知电路输入/输出均接入测量链；重放阶段断开输出端连接，ADC 只采未知输入，PS 使用缓存的复响应离线生成波表，PL 负责稳定循环输出。Levy 拟合和 IIR 只作为学习后的增强项，不作为第一版硬件必需路径。

### 软件实现约束

- 所有硬件访问使用 xparameters.h 生成的 XPAR_* 宏和 typed HAL；禁止新增裸地址。
- DMA 接收完成后，CPU 读取前必须按缓存行对齐执行 invalidate；保留现有 S2MM/TLAST/4096 样本约束。
- 只有新 XSA/BSP 核对通过后，才新增波表基址宏适配；不手工编辑生成的 BSP、xparameters.h 或 Makefile。
- Vitis 落地顺序：HAL/标定 → 基本服务 → 同步解调/学习分类 → 频率估计/波表推断 → 应用状态机与菜单。每一步先编译和小测试，再扩大集成。

## 2026-07-23 实施状态

- 已加入 transfer_function_model、dac_vpp_calibration、coherent_transfer_measurement、rlc_filter_classifier 纯软件层，均不访问硬件寄存器。
- tests/test_transfer_algorithms.c 已用 GCC 严格告警编译并运行通过，覆盖模型幅相、标定正逆插值、I/Q 复频响及基础分类。
- 波表 HAL、DMA 采集闭环和 FreeRTOS 菜单尚未接入；它们必须等新 XSA 导出并核对 XPAR_* 后再实现。

## 2026-07-23 发挥 2 算法状态

fundamental_frequency_estimator 和 response_waveform_generator 已实现为不访问硬件的纯软件层，并通过严格 GCC 主机自检。当前波表生成接受已学习的复响应系数，输出归一化 14-bit 表；实际 PS BRAM 写入仍等待新 XSA/BSP 的 XPAR 证据。
