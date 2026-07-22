# Codex 新会话统一入口

本文件是 2025G 项目的唯一新会话入口，使用仓库相对路径，不依赖盘符、用户名或克隆目录。

## 唯一启动口令

开启 Codex 新会话并把工作目录选到本仓库根目录后，只需发送：

> 请完整读取 CODEX_START.md，严格执行其中的初始化流程；读取完成后先汇报当前状态，不要在未获得本次任务批准前修改代码。

若本轮消息已经明确要求实施某项修改，该项授权有效；其余范围仍遵循 PL/PS 审批工作流。

## 仓库识别

- 仓库根目录是包含本文件、25G_PL/ 和 25G_PS/ 的目录。
- 不得依赖历史绝对路径；工程路径从仓库根目录或脚本位置推导。
- 目标 Git 分支为 calvin。若当前不是该分支，停止并汇报，不得自行切换或覆盖用户修改。
- 2023H 只作为历史参考，不是活动工程。

## 初始化必读顺序

在回答架构、诊断或修改文件前，必须完整阅读：

1. CODEX_START.md。
2. 25G_PL/AGENTS.md。
3. 25G_PS/AGENTS.md。
4. 25G_PL/.ai/ARCHITECTURE.md、CHANGELOG.md、STYLE.md、MEMORY.md。
5. 25G_PS/.ai/PLagriculture.md、architecture.md、CHANGELOG.md、STYLE.md、MEMORY.md。
6. 与本次任务直接相关的全部 RTL、约束、BD、IP component.xml/XCI、Vitis 源码及 BSP xparameters.h；禁止只读片段后下结论。
7. 官方题目 G题_电路模型探究装置.pdf。若 PDF 不可访问，可使用已验证的 .ai 事实进行安全检查；涉及指标解释、图示或原文争议时必须请用户重新附加 PDF，不得猜测。

## 事实优先级

冲突时依次采用并报告冲突：

1. 用户当前明确指令。
2. 25G_PL/AGENTS.md 与 25G_PS/AGENTS.md。
3. 官方题目 PDF。
4. 当前生成的硬件事实：XSA、BSP xparameters.h、Vivado BD/IP 配置。
5. 已通过验证的源码与测试结果。
6. .ai 知识库。
7. 历史工程和静态审查文档。

.ai 是恢复入口，不替代源码、XSA、仿真、综合或板级测量证据。

## 新会话只读检查

先执行并汇报：

- git status --short --branch
- git log -5 --oneline
- git remote -v
- 当前分支与 origin/calvin 的领先/落后关系
- 工作树是否存在用户未提交修改
- Vivado/Vitis 版本和关键输入是否存在

随后严格区分：

- 已实现且已验证
- 已实现但未验证
- 已验证但尚未集成
- 尚未实现

完成这些检查前不得声称系统满足题目。

## 当前恢复点（2026-07-22）

- Vivado 工程为 25G_PL/25G_PL.xpr，器件 Zynq-7020，工具 2020.2。
- XPR 用户 IP 仓库已修正为仓库内 25G_PL/ip_core，Vivado 已确认加载。
- ad_fifo_warpper 与 DAC_DDS_Output 已通过独立编译和行为回归。脚本为 25G_PL/script/pl_test_packaged_ips.tcl，测试平台为 25G_PL/sim/ip/tb_packaged_ips.sv。
- 两个 IP 尚未替换活动顶层，属于“已验证但尚未集成”。
- 活动顶层仍是旧 ADC/FIFO、AXIS/DMA、控制 BRAM和双 DDS 路径。
- 活动 Vitis 主程序仍是 FreeRTOS Hello World，2025G 状态机与算法尚未实现。
- 已批准方向：ADC/DAC 同一板级参考体系、独立 4096x14 任意波 RAM、原子 DDS 控制、相干学习、复响应表和稳定同频重放。
- 绝不擅自修改现有接口名称或参数默认值。

## 推荐继续顺序

1. 保持旧顶层可回退，完成两个 IP 的顶层接口适配。
2. 集成独立任意波 RAM及 PS 写/DDS 读契约。
3. 扩展 PL 回归，再完成综合、实现、时序和 XSA。
4. 读取新 XSA 的 xparameters.h 后实现 Vitis HAL与状态机。
5. 依次完成基础 DDS、RLC 学习分类、周期信号推断重放和板级校准。
6. 每个真实门槛通过后立即更新对应 .ai。

## 不可破坏的契约

- 控制 BRAM 的 COMMIT_SEQ 永远最后写。
- 任意波表使用独立 RAM，不占用十字控制 BRAM。
- PS 只使用 BSP 的 XPAR_* 标识。
- DMA 接收完成后、CPU 读取前执行缓存失效；发送数据按需要刷新。
- 不修改生成 BSP 驱动和生成 Makefile。
- 仿真、综合、时序与板测是不同门槛，不能互相替代。
- 仿真不能证明输入阻抗、模拟幅度、抗混叠、DAC 重构或 1 MHz 板级性能。

## Git 与交接

- 保留用户已有修改，不使用破坏性重置。
- 只提交本任务文件；提交前运行 git diff --check 和相应验证。
- 按 AGENTS 规则，在关键修改验证后提交并推送 calvin；推送前确认远端无冲突。
- .ai 只记录事实、验证结果和未完成边界，不把计划写成已完成。
- 若本文件或 .ai 落后于源码/Git，先核验并更新知识库。
