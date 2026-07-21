# Language Policy

本工程允许 Verilog 与 SystemVerilog 共存。

原则：

- 保持模块原有语言，不为了统一而重写。
- 新模块优先使用 SystemVerilog。
- 修改已有模块时，保持原文件风格。
- 不允许为了使用 SV 特性而大规模重构 Verilog 模块。

## Verilog

允许：

- reg
- wire
- parameter
- always @(*)
- always @(posedge clk)

## SystemVerilog

允许：

- logic
- always_ff
- always_comb
- typedef
- enum
- package
- interface（仅在确有必要时）