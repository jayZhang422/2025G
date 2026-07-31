# 26G J11 串口屏上板验收记录

本表只验收最终 J11/AXI UARTLite 串口屏链路。J12/UART0 已放弃，不得接线或拿其
旧 ELF 代替。当前交付不包含 DAC，也不生成 `BOOT.BIN`。

## 1. 受测版本

| 项目 | 记录值 |
| --- | --- |
| 日期/时间 |  |
| 操作者 |  |
| FPGA 板卡序列或标识 |  |
| 屏幕型号/标识 |  |
| 源码提交 | `5b6ecdf27767cbd1aafda96ecba92069a024c922` |
| bit SHA-256 | `4DD2EB78924B75C59FCCCE057F498259DB5313661C59232B461AE3EAE501ADEF` |
| XSA SHA-256 | `1F848289D326ED229101076766660BC64DD8E76E406ECA18B022ECE408A3C14C` |
| ELF SHA-256 | `7C4B8BE03253911F571FECECB91F530316143D5942A81F32742D9AD430748D16` |
| ps7_init SHA-256 | `3C4A9FF99EC83AB9C4A8CB98B21C3952AF484E809E050AA075FB151642BCB74D` |
| HMI SHA-256 | `8636C95B4500E266F8FC5F5D08DE8B9B841051F43867E64BADF5EB6249858AF7` |
| PS UART1 端口 |  |
| 录像/照片/串口日志路径 |  |

先在移交包根目录核对：

```powershell
Get-FileHash -Algorithm SHA256 `
  .\artifacts\25g_2026g_hmi_j11.bit, `
  .\artifacts\25g_2026g_hmi_j11.xsa, `
  .\artifacts\hmi_candidate_app.elf, `
  .\artifacts\ps7_init.tcl, `
  .\hmi\26G.HMI
```

## 2. 断电接线与电气检查

| 检查项 | 实测/结果 | PASS/FAIL |
| --- | --- | --- |
| J11 PIN3/F17 -> 屏幕 RX |  |  |
| 屏幕 TX -> 降压/保护 -> J11 PIN4/F16 |  |  |
| J11 PIN1 与屏幕 GND 共地 |  |  |
| J11 PIN2 未给屏幕供电 |  |  |
| 适配模块 USB 已断开 |  |  |
| 降压前屏幕 TX 高电平 |  V |  |
| 降压后送入 F16 的高电平 |  V |  |
| 降压后低电平 |  V |  |
| 屏幕已烧入匹配 `26G.HMI`，115200 8N1 |  |  |

没有确认 F16 输入电平安全之前，不得给 FPGA 和屏幕上电联调。

## 3. 无 FSBL JTAG 下载

从移交包根目录执行：

```powershell
$env:PS_HMI_CANDIDATE_BIT = `
  (Resolve-Path '.\artifacts\25g_2026g_hmi_j11.bit')
$env:PS_HMI_CANDIDATE_XSA = `
  (Resolve-Path '.\artifacts\25g_2026g_hmi_j11.xsa')
$env:PS_HMI_CANDIDATE_PS7_INIT = `
  (Resolve-Path '.\artifacts\ps7_init.tcl')
$env:PS_HMI_CANDIDATE_ELF = `
  (Resolve-Path '.\artifacts\hmi_candidate_app.elf')
& 'D:\Vivado_install\Vitis\2020.2\bin\xsct.bat' `
  '.\source_tree\25G_PS\script\ps_run_hmi_candidate.tcl'
```

PS UART1 打开 115200 8N1、无流控。保存完整原始日志，不只截取 PASS 行。

| 启动检查 | 结果 | PASS/FAIL |
| --- | --- | --- |
| XSCT 识别 APU/A9，bit 下载成功 |  |  |
| 串口出现 `[HMI] READY: J11 UARTLite 115200 8N1` |  |  |
| 没有 `[HMI] FATAL` |  |  |

## 4. 双向通信与页面功能

| 用例 | 操作与期望 | 实测结果/证据 | PASS/FAIL |
| --- | --- | --- | --- |
| page2 START | 触摸 START；日志出现 `page=2 command=0x01`，完成一次测量并显示 1 周期波形 |  |  |
| 重复 START | 第一次尚未完成时再次触摸；只允许一个有效 generation，应出现 `START_IGNORED` |  |  |
| page2 STOP | 触摸 STOP；日志出现 `page=2 command=0x00`，旧完成结果不得重新刷新页面 |  |  |
| 1 周期 | 触摸 1 周期；画面稳定显示恰好 1 个完整周期 |  |  |
| 3 周期 | 触摸 3 周期；画面稳定显示恰好 3 个完整周期 |  |  |
| 时域参数 | 显示 Upp、真 RMS、基频，单位和数值位置正确 |  |  |
| page3 START | 日志出现 `page=3 command=0x01`，显示正频率轴离散线谱 |  |  |
| 2 条谱线 | 频率位置和相对高度正确，第三行隐藏 |  |  |
| 3 条谱线 | 三条谱线、三行频率正确，第三行显示 |  |  |
| 幅值按钮 | 日志出现 `page=3 command=0x11`；显示各分量峰值幅度，不是峰峰值 |  |  |
| page3 STOP | 日志出现 `page=3 command=0x00`，页面清理且可再次 START |  |  |
| 页面往返 | page2/page3 往返后，触摸和显示仍正常，没有旧 generation 串页 |  |  |

## 5. 两秒与稳定性验收

题目计时口径是“触摸 START 到屏幕最终画面稳定”，不是算法时间，也不是
`DISPLAY_TX_COMPLETE` 单独的日志时间。建议录像逐帧取触摸时刻与最终画面时刻。

| 轮次 | 页面/信号 | 日志 elapsed (ms) | 录像端到端 (ms) | PASS/FAIL |
| ---: | --- | ---: | ---: | --- |
| 1 | page2 |  |  |  |
| 2 | page2 |  |  |  |
| 3 | page2 |  |  |  |
| 4 | page3 |  |  |  |
| 5 | page3 |  |  |  |

每次端到端时间必须小于 2000 ms。连续执行至少 10 次 START/STOP/页面切换，记录
丢帧、乱码、页面卡死、错误日志或显示残留；任何一次失败都保留原始日志后再诊断。

## 6. 验收结论

- [ ] 最终 bit/XSA/ps7_init/ELF/HMI 哈希全部匹配。
- [ ] F16 输入电平安全，USB-UART 不会并联争用。
- [ ] J11 双向触摸和显示通过。
- [ ] 1/3 周期、时域参数、线谱和幅值页面通过。
- [ ] 重复 START、STOP 和跨页 generation 隔离通过。
- [ ] 实际触摸到最终显示连续多次小于 2 秒。
- [ ] 完整日志、照片或录像已归档。

最终结论：`未验收 / PASS / FAIL`（保留一个并填写原因）：
