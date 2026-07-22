# ALINX AX7020 开发板核心

## 1. 核心硬件参数
* **开发板型号**: ALINX AX7020  
* **核心 ZYNQ 芯片**: XC7Z020-2CLG400I (FBGA封装，400引脚，包含双核 ARM Cortex-A9 和 FPGA 可编程逻辑)  
* **DDR3 型号**: 2片 SK hynix `H5TQ4G63AFR-PBC` (兼容 MT41J256M16RE-125) 
  * **容量**: 每片 4Gbit，总容量 8Gbit (1GB)  
  * **总线宽度**: 32 bit  
  * **最高速率**: 533MHz (数据速率 1066Mbps)  
* **QSPI FLASH**: `W25Q256` (容量 32M Byte / 256Mbit，Winbond，用于存储启动镜像和文件)  
* **以太网 PHY**: Realtek `RTL8211E-VL` (支持 10/100/1000 Mbps，通过 RGMII 接口通信) 
* **USB 2.0 芯片**: `USB3320C-EZK` (1.8V 高速 ULPI 接口，支持 Host / Slave 模式) 
* **USB 转串口芯片**: Silicon Labs `CP2102GM` (通过 Micro USB 与 PC 通信) 
* **EEPROM**: `24LC04` (容量 4Kbit，IIC 接口通信) 
* **实时时钟 (RTC)**: `DS1302` (配有 CR1220 纽扣电池座，外接 32.768KHz 晶振) 

---

## 2. 时钟配置引脚

### 2.1 PS 端时钟
| 时钟源 | 频率 | 信号名称 | ZYNQ 引脚名 | ZYNQ 引脚号 | 备注 |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **PS 系统时钟** | 33.333MHz   | PS_CLK_500 | PS_CLK_500 | E7   | 为 ARM 系统提供稳定时钟 |

### 2.2 PL 端时钟
| 时钟源 | 频率 | 信号名称 | ZYNQ 引脚号 | 备注 |
| :--- | :--- | :--- | :--- | :--- |
| **PL 系统时钟** | 50MHz   | PL_GCLK | U18   | 驱动 FPGA 全局时钟 (MRCC)   |

---

## 3. PS 端外设引脚分配

### 3.1 基础交互外设 (PS LED & 按键)
| 信号名称 | ZYNQ 引脚名 | ZYNQ 引脚号 | 备注 |
| :--- | :--- | :--- | :--- |
| **MIO0_LED (LED1)** | PS_MIO0_500 | E6   | PS 控制的 LED1 |
| **MIO13_LED (LED2)** | PS_MIO13_500 | E8   | PS 控制的 LED2 |
| **MIO_KEY1** | PS_MIO50_501 | B13   | PS 用户按键1 |
| **MIO_KEY2** | PS_MIO51_501 | B9   | PS 用户按键2 |

### 3.2 串行通信与存储 (UART / SD / SPI)
| 接口类型 | 信号名称 | ZYNQ 引脚名 | ZYNQ 引脚号 | 备注 |
| :--- | :--- | :--- | :--- | :--- |
| **UART** | UART_TX | PS_MIO48_501 | B12 | Uart 数据输出 |
| **UART** | UART_RX | PS_MIO49_501 | C12 | Uart 数据输入 |
| **SD 卡** | SD_CLK | PS_MIO40 | D14 | SD时钟信号 |
| **SD 卡** | SD_CMD | PS_MIO41 | C17 | SD命令信号 |
| **SD 卡** | SD_D0 | PS_MIO42 | E12 | SD数据0 |
| **SD 卡** | SD_CD | PS_MIO47 | B14 | SD卡插入信号 |
| **QSPI Flash** | QSPI_CLK | PS_MIO6_500 | A5 | QSPI 时钟 |
| **QSPI Flash** | QSPI_CS | PS_MIO1_500 | A7 | QSPI 片选 |
| **QSPI Flash** | QSPI_D0 | PS_MIO2_500 | B8 | QSPI 数据0 |

### 3.3 PS 端 PMOD 扩展口引脚映射 (J12)
12针 2.54mm 间距接口，连接至 PS BANK500 (3.3V 电平标准)。  

| 管脚号 | 信号名称 | ZYNQ 引脚名 | ZYNQ 引脚号 | 管脚号 | 信号名称 | ZYNQ 引脚名 | ZYNQ 引脚号 |
| :---: | :--- | :--- | :---: | :---: | :--- | :--- | :---: |
| **1** | PMOD_IO0 | PS_MIO11_500 | C6   | **7** | PMOD_IO1 | PS_MIO10_500 | E9   |
| **2** | PMOD_IO2 | PS_MIO9_500 | B5   | **8** | PMOD_IO6 | PS_MIO8_500 | D5   |
| **3** | PMOD_IO3 | PS_MIO15_500 | C8   | **9** | PMOD_IO7 | PS_MIO14_500 | C5   |
| **4** | PMOD_IO4 | PS_MIO7_500 | D8   | **10** | PMOD_IO5 | PS_MIO12_500 | D9   |
| **5** | GND | - | -   | **11** | GND | - | -   |
| **6** | +3.3V | - | -   | **12** | +3.3V | - | -   |

---

## 4. PL 端外设引脚分配

### 4.1 基础交互外设 (PL LED & 按键)
| 信号名称 | ZYNQ 引脚号 | 备注 |
| :--- | :--- | :--- |
| **LED1** | M14   | PL 用户 LED1 |
| **LED2** | M15   | PL 用户 LED2 |
| **LED3** | K16   | PL 用户 LED3 |
| **LED4** | J16   | PL 用户 LED4 |
| **KEY1** | N15   | PL 用户按键1 |
| **KEY2** | N16   | PL 用户按键2 |
| **KEY3** | T17   | PL 用户按键3 |
| **KEY4** | R17   | PL 用户按键4 |

### 4.2 串行通信 (EEPROM / RTC)
| 接口类型 | 信号名称 | ZYNQ 引脚号 | 备注 |
| :--- | :--- | :--- | :--- |
| **EEPROM** | EEPROM_I2C_SCL | T19 | IIC 时钟信号 |
| **EEPROM** | EEPROM_I2C_SDA | U19 | IIC 数据信号 |
| **RTC** | RTC_SCLK | R19 | RTC 时钟信号 |
| **RTC** | RTC_DATA | L14 | RTC 数据信号 |
| **RTC** | RTC_RESET | L15 | RTC 复位信号 |

### 4.3 PL 端 J10 扩展口引脚映射 (1~40)
40管脚 2.54mm 双排连接器，连接至 PL BANK34/35。包含差分对走线 (N/P)。  

| 管脚号 | 信号名称 | ZYNQ 引脚号 | 管脚号 | 信号名称 | ZYNQ 引脚号 |
| :---: | :--- | :---: | :---: | :--- | :---: |
| **1** | GND | -   | **2** | +5V | -   |
| **3** | EX_IO1_1N | W19   | **4** | EX_IO1_1P | W18   |
| **5** | EX_IO1_2N | R14   | **6** | EX_IO1_2P | P14   |
| **7** | EX_IO1_3N | Y17   | **8** | EX_IO1_3P | Y16   |
| **9** | EX_IO1_4N | W15   | **10** | EX_IO1_4P | V15   |
| **11** | EX_IO1_5N | Y14   | **12** | EX_IO1_5P | W14   |
| **13** | EX_IO1_6N | P18   | **14** | EX_IO1_6P | N17   |
| **15** | EX_IO1_7N | U15   | **16** | EX_IO1_7P | U14   |
| **17** | EX_IO1_8N | P16   | **18** | EX_IO1_8P | P15   |
| **19** | EX_IO1_9N | U17   | **20** | EX_IO1_9P | T16   |
| **21** | EX_IO1_10N | V18   | **22** | EX_IO1_10P | V17   |
| **23** | EX_IO1_11N | T15   | **24** | EX_IO1_11P | T14   |
| **25** | EX_IO1_12N | V13   | **26** | EX_IO1_12P | U13   |
| **27** | EX_IO1_13N | W13   | **28** | EX_IO1_13P | V12   |
| **29** | EX_IO1_14N | U12   | **30** | EX_IO1_14P | T12   |
| **31** | EX_IO1_15N | T10   | **32** | EX_IO1_15P | T11   |
| **33** | EX_IO1_16N | A20   | **34** | EX_IO1_16P | B19   |
| **35** | EX_IO1_17N | B20   | **36** | EX_IO1_17P | C20   |
| **37** | GND | -   | **38** | GND | -   |
| **39** | +3.3V | -   | **40** | +3.3V | -   |

### 4.4 PL 端 J11 扩展口引脚映射 (1~40)
40管脚 2.54mm 双排连接器，连接至 PL BANK35。包含差分对走线 (N/P)。  

| 管脚号 | 信号名称 | ZYNQ 引脚号 | 管脚号 | 信号名称 | ZYNQ 引脚号 |
| :---: | :--- | :---: | :---: | :--- | :---: |
| **1** | GND | -   | **2** | +5V | -   |
| **3** | EX_IO2_1N | F17   | **4** | EX_IO2_1P | F16   |
| **5** | EX_IO2_2N | F20   | **6** | EX_IO2_2P | F19   |
| **7** | EX_IO2_3N | G20   | **8** | EX_IO2_3P | G19   |
| **9** | EX_IO2_4N | H18   | **10** | EX_IO2_4P | J18   |
| **11** | EX_IO2_5N | L20   | **12** | EX_IO2_5P | L19   |
| **13** | EX_IO2_6N | M20   | **14** | EX_IO2_6P | M19   |
| **15** | EX_IO2_7N | K18   | **16** | EX_IO2_7P | K17   |
| **17** | EX_IO2_8N | J19   | **18** | EX_IO2_8P | K19   |
| **19** | EX_IO2_9N | H20   | **20** | EX_IO2_9P | J20   |
| **21** | EX_IO2_10N | L17   | **22** | EX_IO2_10P | L16   |
| **23** | EX_IO2_11N | M18   | **24** | EX_IO2_11P | M17   |
| **25** | EX_IO2_12N | D20   | **26** | EX_IO2_12P | D19   |
| **27** | EX_IO2_13N | E19   | **28** | EX_IO2_13P | E18   |
| **29** | EX_IO2_14N | G18   | **30** | EX_IO2_14P | G17   |
| **31** | EX_IO2_15N | H17   | **32** | EX_IO2_15P | H16   |
| **33** | EX_IO2_16N | G15   | **34** | EX_IO2_16P | H15   |
| **35** | EX_IO2_17N | J14   | **36** | EX_IO2_17P | K14   |
| **37** | GND | -   | **38** | GND | -   |
| **39** | +3.3V | -   | **40** | +3.3V | -   |