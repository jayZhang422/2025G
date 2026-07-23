# 2025G Vivado Architecture

This is the authoritative PL context for new sessions. Target items are not implemented until a verification entry says so.

## Project

- Repository branch: `calvin`; Vivado project: `25G_PL.xpr`.
- Zynq-7020 `xc7z020clg400-2`, Vivado 2020.2.
- Synthesis top `top`; simulation top `tb_H_top`.
- Preserve existing interface names and parameter defaults unless explicitly approved.

## Requirement facts

The known model is `H(s)=5/(1e-8*s^2+3e-4*s+1)`. Its poles are about 607.9 Hz and 4166.7 Hz. At 3 kHz `|H| ~= 0.806`, so a 2 Vpp requested output needs about 2.48 Vpp input, not 12.4 Vpp.

The device must provide calibrated sine output through at least 1 MHz, learn an unknown one-R/one-L/one-C network in under 2 minutes, classify four filter types, and reproduce the response to 1-50 kHz periodic inputs within 5 seconds without visible frequency drift.

## Verified baseline

The active hierarchy still uses legacy RTL; the two packaged IPs exist but are not instantiated.

```text
AD9226 -> 5.1208 MHz capture -> async FIFO
-> 100 MHz AXIS (4096 accepted beats/TLAST)
-> AXIS FIFO -> simple S2MM DMA -> PS DDR

PS GP0 -> AXI BRAM controller -> ten-word control BRAM
-> atomic COMMIT_SEQ apply -> dual 125 MHz DDS -> AD9767 A/B
```

The DMA is S2MM-only and polled. Control BRAM and DMA identifiers must come from BSP `XPAR_*` symbols.

## Stable control-BRAM contract

| Offset | Meaning |
| --- | --- |
| 0x00/04/08/0C | A waveform/step/phase/amplitude |
| 0x10/14/18/1C | B waveform/step/phase/amplitude |
| 0x20 | RUN, PHASE_RELOAD and approved extension bits |
| 0x24 | COMMIT_SEQ, written last |

A changed sequence is applied only after a complete PL shadow scan. STOP is a valid commit and forces DAC midscale.

## Packaged IPs

- `ad_fifo_warpper` packages ADC registration and the async FIFO. Keep the existing spelling. It needs an external phase clock and lacks physical ADC clock generation and PLL-lock gating, so it is not a drop-in replacement.
- `DAC_DDS_Output` keeps defaults `32/12/14`, the atomic protocol and dual DDS. Waveform value 2 selects external arbitrary memory. Sine, triangle and arbitrary memories are outside the core; their ports and one-cycle latency must be integrated before replacement.

## Approved target

```text
common board reference
|- ADC clocking -> ADC/FIFO -> AXIS/DMA -> PS
`- DAC clocking -> DDS/arbitrary replay -> AD9767

control BRAM ----------------------> atomic DDS configuration
dedicated 4096x14 wave BRAM ------> arbitrary waveform reader
```

ADC and DAC clocks must derive from one board reference so measured frequency replays without oscillator-ratio drift. Use a dedicated PS-write/DDS-read waveform RAM; do not store the table in the 8 KiB control BRAM. Start with one bank because DDS is stopped while the table is built.

Analog requirements are board responsibilities: >=100 kOhm buffered input, ADC bias/protection/anti-aliasing, controlled input/output measurement selection, DAC reconstruction/gain and measured code-to-Vpp calibration.

## Verification gates

1. Fresh-clone script/path and BD validation.
2. Existing DDS/AXIS behavioral regression.
3. Packaged-IP compile and equivalence tests.
4. Arbitrary-wave RAM replay test.
5. Synthesis, implementation, timing and XSA export.
6. Vitis build against the exported XSA.
7. Board amplitude, impedance, coherent-frequency and end-to-end tests.

Static inspection alone never passes a gate.


## Verified progress - 2026-07-22

- script/pl_update_bd.tcl --check passed in Vivado 2020.2 from a fresh local calvin clone.
- Vivado loaded the repository-local user IP catalog at ip_core.
- script/pl_test_packaged_ips.tcl compiled DAC_DDS_Output, ad_fifo_warpper, ad9226, fifo, and FIFO Generator 13.2, then ran sim/ip/tb_packaged_ips.sv.
- XSim printed PACKAGED_IP_REGRESSION_PASSED and called $finish at 2506 ns.
- Regression covers DDS shadow isolation, arbitrary-mode atomic commit, phase reload, step advance, DAC falling-phase update, atomic stop/midscale, ADC stabilization, FIFO threshold release, changing samples, and 12-bit high alignment.
- This passes verification gate 3 only. Packaged IPs remain absent from the active top-level hierarchy; gates 4 through 7 remain open.

## 2026-07-23 执行架构：已知模型、学习与波表重放

本节是本题当前执行方案；它记录设计边界和验证顺序，不把尚未通过工程验证的集成写成既成事实。

### 功能分层

- 基本 1：已知模型电路用扫频测量与理论 H(s) 对照，软件侧由 known_model.[ch] 负责模型与比较。
- 基本 2：1 MHz 输出使用 125 MHz 时钟 DDS 相位累加器，step = round(f * 2^32 / 125 MHz)；幅度由 DAC 离线标定。
- 基本 3：1 kHz、2 Vpp 先计算 |H(j2πf)|，令 Vin,pp = Vout,target,pp / |H(j2πf)|，再由标定曲线换算 DDS 幅度码。
- 基本 4：100 Hz–3 kHz、1–2 Vpp 沿用同一开环模型与标定流程，频率和目标幅度参数化；启动前一次性设置，运行中不使用 ADC/PID 反馈。
- 发挥 1：未知 RLC 学习阶段逐点扫频；ADC 对每个频点同步解调得到复数 H_k，再分类为低通/高通/带通/带阻。对应软件模块为 coherent_measure.[ch]、rlc_learning.[ch]、filter_classifier.[ch]。
- 发挥 2：周期输入重放分为“捕获输入→估计基频→得到响应系数→逆变换生成 4096 点波表→DDS 循环播放”，对应 frequency_estimator.[ch]、waveform_inference.[ch]、wave_ram.[ch]。

同步解调定义为：

I = (2/N) Σ y[n] cos(2π f n/fs)
Q = -(2/N) Σ y[n] sin(2π f n/fs)
Y(f) = I + jQ
H(f) = Y(f) / X(f)

保留复数增益和相位；基本 3/4 的目标输出严格走开环离线标定，不连接已知模型输出端到探测装置，也不引入运行时 PID、采样闭环或 DMA 控制回路。

### RTL 集成顺序与边界

1. 以当前已验证的生成 system_wrapper.v 为唯一 XPR 活动 wrapper；保留旧 wrapper 作为回退参考。
2. 先修正并独立编译 reviewed 的 g2025_top_v3 + g2025_dac_adapter_v3 路径，再切换 XPR 顶层；不得把 v1/v2 探索文件混入活动源集。
3. 波表 BRAM 物理边界固定为 4096×32、字节地址、4-bit 写使能；样本放在低 14 位，PL 只读，控制 BRAM 既有接口和默认参数不变。
4. 通过 RTL/仿真后再运行综合、实现并生成新 XSA；在新 XSA 和 xparameters.h 核对前，不实现 PS 波表 HAL 或硬编码波表地址。

### 验证门

- Gate A：接口、位宽、wrapper 端口和 XPR 源集静态核对。
- Gate B：DDS/波表适配器隔离仿真，检查停止、中点、原始波形选择、原子提交和一拍同步 BRAM 读。
- Gate C：活动顶层 RTL 编译/仿真，检查 ADC/AXIS/TLAST、旧控制 BRAM 和 DAC 波表路径。
- Gate D：Vivado 综合/实现/bitstream/XSA；随后更新平台、BSP 与 XPAR。
- Gate E：Vitis 先实现 HAL 和离线标定/基本服务，再实现学习与推断，最后接入菜单和 FreeRTOS 状态机；每层都要有日志或离线测试证据。

## 2026-07-23 实施状态

- g2025_top_v3.sv 已修正为连接 generated wrapper 的 DDR_we_n 端口；旧 top 和旧 wrapper 未删除。
- script/pl_select_g2025_top_v3.tcl 已准备好，要求 generated wrapper 存在，加入 reviewed v3 顶层、适配器和 DDS RTL，并提供 --check、--compile-only 与正式切换入口。
- XPR 当前活动顶层仍是旧 top；在 Vivado Gate C 通过前，不宣称波表路径已经集成。
