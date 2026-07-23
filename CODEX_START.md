# Codex 新会话统一入口

本文件只承担“恢复工程所需的核心事实和阅读路由”，不替代源码、Vivado/Vitis 生成物、验证日志或题目 PDF。所有路径均相对仓库根目录。

## 核心恢复必读

每个新会话先：

1. 完整读取本文件。
2. 只读检查 `git status --short --branch`、`git log -5 --oneline`、`git remote -v`，并确认当前分支、与 `origin/calvin` 的领先/落后关系及用户未提交修改。
3. 确认 Vivado/Vitis 版本和本任务所需的 XPR、XSA、BSP `xparameters.h` 等输入是否存在。

当前仓库识别：根目录同时包含本文件、`25G_PL/`、`25G_PS/`；目标分支为 `calvin`。若当前不是该分支，停止并汇报，不得自行切换或覆盖用户修改。`2023H` 仅作历史参考。

## 按任务展开阅读

不要为无关任务读取整个工程，只沿任务边界展开：

- PL 任务：读取 `25G_PL/AGENTS.md`、相关的 `25G_PL/.ai/ARCHITECTURE.md`、`STYLE.md`、`MEMORY.md`、最新相关 `CHANGELOG.md`/handoff，以及直接相关的全部 RTL、XDC、BD、XCI/component.xml 和脚本。
- PS/Vitis 任务：读取 `25G_PS/AGENTS.md`、`25G_PS/.ai/PLagriculture.md`、相关 architecture/style/memory/changelog，以及对应 XSA、BSP `xparameters.h`、Vitis 源码和构建输入。
- 跨 PL/PS 任务：合并以上两条阅读范围，并先确认硬件事实再设计软件接口。
- 题目指标、图示或原文争议：读取官方 `G题_电路模型探究装置.pdf`；没有 PDF 时不得猜测争议内容。普通代码/文档任务不因启动而读取 PDF。

相关文件必须完整读取；不得只看片段后诊断、回答架构问题或修改文件。代码修改必须在本轮任务明确授权后进行。

## 事实优先级与不可破坏契约

冲突时依次采用：用户当前指令、PL/PS `AGENTS.md`、官方题目 PDF、当前 XSA/BSP/Vivado BD/IP 事实、已验证源码与测试、`.ai`、历史文档。`.ai` 是恢复索引，不替代硬件和验证证据。

- 不猜测；不擅自改接口名或参数默认值。
- 控制 BRAM 的 `COMMIT_SEQ` 永远最后写；波表使用独立 RAM，不占十字控制 BRAM。
- PS 硬件标识只使用 BSP 的 `XPAR_*`；DMA 接收完成后、CPU 读取前必须失效缓存。
- 不修改生成 BSP 驱动和生成 Makefile；保留用户已有修改，不使用破坏性重置。
- 仿真、综合、实现、时序、XSA/Vitis 和板测是不同门槛，不能互相替代。未通过相应门槛不得声称系统满足题目。

## 当前恢复点（2026-07-23）

- Vivado/Vitis 2020.2；工程为 `25G_PL/25G_PL.xpr`，器件 Zynq-7020。
- XPR 用户 IP 仓库为仓库内 `25G_PL/ip_core`。
- `bf0cb7b` 已将 XPR 活动 wrapper 保存为生成文件
  `25G_PL.gen/sources_1/bd/system/hdl/system_wrapper.v`；历史导入 wrapper 仍可在磁盘保留，但不是活动 XPR 条目。
- `axi_bram_ctrl_wave` / `blk_PS_TO_PL_WAVE` 已进入 BD；PS 映射为 `0x4200_0000`、范围 `0x4000`，4096 个 32 位字的低 14 位保存样本。`COMMIT_SEQ` 仍属于独立控制 BRAM。
- 打包 `ad_fifo_warpper` 与 `DAC_DDS_Output` 已通过独立编译/行为回归；它们仍未替换活动顶层。波表资源和适配代码也不能据此视为已完成系统集成。
- 活动顶层仍是旧 ADC/FIFO、AXIS/DMA、控制 BRAM 和双 DDS 路径；活动 Vitis 主程序仍为 FreeRTOS Hello World，2025G 状态机、学习、推断、校准和端到端板测尚未完成。

状态汇报必须分为：已实现且已验证、已实现但未验证、已验证但尚未集成、尚未实现。每个结论注明对应证据和边界。

## 交接与提交

保持顶层可回退；只提交本任务文件。提交前运行 `git diff --check` 和相应验证。关键修改通过验证后，按 AGENTS 规则确认远端无冲突，再提交并推送 `calvin`。`.ai` 只记录事实、验证结果和未完成边界，不把计划写成已完成。