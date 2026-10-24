# STM32F103ZET6 多传感器 LoRa 上报工程 —— 从机

> 最后更新：2026-10-24
> 编译状态：**0 Error / 3 Warning**（3 个警告均来自 `i2c.c` 的整数溢出，是原工程遗留，不影响功能）
> 固件：`project/Objects/STM32F103ZET6_Project.hex`（35585 字节）
> 角色：**从机（发送端）** —— 采集三路传感器，组帧后经 LoRa 发给主机

---

## 一、工程功能

采集 **雨滴传感器 + 水位传感器 + DHT20 温湿度**，打包后通过 **AS32-TTL-100-C LoRa 模块**无线发送，同时在本地串口打印中文关键数据。

```
┌─────────────────┐
│ 雨滴传感器       │ PC1(DO) / PC2(AO, ADC1_IN12)
├─────────────────┤
│ 水位传感器       │ PA1(AO, ADC1_IN1)  仅 AO，不接 DO
├─────────────────┤
│ DHT20 温湿度     │ PB6(SCL) / PB7(SDA)  软件 I2C
└────────┬────────┘
         │ 组帧
         ▼
   LORA_SendData() ──► USART3(PB10/PB11) ──► LoRa 天线 ──► 主机
         │
         └──► USART1(PA9/PA10) ──► PC 串口助手（中文可读）
```

**上报周期：1 秒（实时上报）**

---

## 二、硬件接线

### 单片机引脚分配总表

| 功能 | 引脚 | 外设/模式 | 备注 |
|---|---|---|---|
| **雨滴 DO** | **PC1** | 上拉输入 | 1=无水，0=有水 |
| **雨滴 AO** | **PC2** | 模拟输入 | ADC1_IN12，水越多电压越低 |
| **水位 AO** | **PA1** | 模拟输入 | ADC1_IN1，**需拔 P10 跳线帽** |
| DHT20 SCL | PB6 | 开漏输出 | 软件 I2C，需 4.7K~10K 上拉 |
| DHT20 SDA | PB7 | 开漏/浮空 | 软件 I2C，需 4.7K~10K 上拉 |
| LoRa TXD→ | PB11 | USART3_RX | 模块 TXD 接单片机 RX |
| LoRa RXD← | PB10 | USART3_TX | 模块 RXD 接单片机 TX |
| LoRa MD0 | PB12 | 开漏输出 | 模式选择低位 |
| LoRa MD1 | PB13 | 开漏输出 | 模式选择高位 |
| LoRa AUX | PB14 | 浮空输入 | 模块忙/闲状态 |
| 调试串口 | PA9/PA10 | USART1 | 9600 8N1，接 PC |
| LED0 / LED1 | PB5 / PE5 | 推挽输出 | 低电平点亮 |
| 蜂鸣器 | PB8 | 推挽输出 | |

### 传感器接线

**雨滴传感器**
```
VCC → 3.3V
GND → GND
DO  → PC1
AO  → PC2
```

**水位传感器（XPT2046 液位转换板）**
```
VCC → 5V          ← 注意是 5V，不是 3.3V
GND → GND
AO  → PA1         ← 必须拔掉 P10 跳线帽！
DO  → 不接        ← 本工程只读 AO 模拟量，不占用 PA4
```

> **为什么不用 DO**：参考例程把 DO 接 PA4，但 PA4 是 DAC_OUT1 / WIFI复位 /
> 蓝牙EN，属于无线模块接口区，容易和 LoRa 模块干涉，因此本工程不使用 DO，
> 水位判定完全靠 AO 的 ADC 值换算，精度足够。

**LoRa 模块（AS32-TTL-100-C）**
```
VCC → 3.3V~5V（发射瞬间需 200mA）
GND → GND
TXD → PB11
RXD → PB10
MD0 → PB12
MD1 → PB13
AUX → PB14
```

---

## 三、串口输出格式

**串口助手设置：9600 8N1，编码选 GBK**

### 正常输出（每 1 秒一行）

```
温度:26.4℃,湿度:49.6%,无雨,水位:80%(ADC=716)
温度:26.4℃,湿度:49.6%,无雨,水位:80%(ADC=709)
温度:26.5℃,湿度:52.2%,无雨,水位:80%(ADC=689)
```

> 括号里的 `ADC=xxx` 是水位传感器原始 ADC 值，用于现场校准门限表。
> 如果觉得串口太吵，可在 `sensor_report.c` 的 `Report_PrintCN()` 里去掉这段。

### 异常输出

```
温度:--℃,湿度:--%,无雨,水位:--          ← DHT20 和水位都异常
[错误] DHT20 触发命令无应答(检查 SCL=PB6 SDA=PB7 接线与上拉)
```

### 上电启动信息

```
======== AS32-TTL-100-C LoRa + RainDrop + DHT20 ========
LORA CONFIG : C0 00 00 1A 3D 40
LORA VERSION : AS32-TTL-100-C-V9.0
LORA CONFIG SET OK

雨滴传感器 + DHT20 温湿度 LoRa 上报
雨滴: DO=PC1  AO=PC2  |  DHT20: SCL=PB6  SDA=PB7
每 1 秒上报一次

Start sampling, report via LoRa every 1 seconds
```

---

## 四、LoRa 发送帧格式

从机每秒通过 LoRa 发出一帧 **ASCII 文本帧**：

```
$RAIN,DHT20,T=26.5,H=52.2,ADC=4092,V=3297,WET=0,DO=1,LEVEL=0,WL=80*8B\r\n
```

| 字段 | 含义 |
|---|---|
| `$RAIN,DHT20,` | 固定帧头 |
| `T=` | 温度 ℃，保留一位小数；DHT20 失败时为 `--` |
| `H=` | 湿度 %RH，保留一位小数；失败时为 `--` |
| `ADC=` | 雨滴 AO 原始 ADC 值 0~4095 |
| `V=` | 雨滴 AO 电压 mV |
| `WET=` | 雨滴湿润百分比 0~100 |
| `DO=` | 雨滴数字量 1=无水 0=有水 |
| `LEVEL=` | 雨情等级 0=无雨 1=小雨 2=中雨 3=大雨 |
| `WL=` | **水位百分比 0~100**（本工程新增字段，供主机显示） |
| `*XX` | 校验和：`*` 之前所有字节累加取低 8 位，两位大写 HEX |
| `\r\n` | 帧尾 |

帧长约 **71~79 字节**。

> **⚠️ 重要**：AS32-TTL-100-C 模块收到空口数据后，会按 **32 字节一块**
> 分块往串口吐，块之间会触发 IDLE 中断。所以主机端**必须做流式拼帧**
> （按 `\r\n` 找帧边界），不能把每次 IDLE 收到的数据当一帧。
> 主机工程里的 `frame_assembler.c` 就是干这个的。

---

## 五、判定逻辑

### 雨情分档（按雨滴 AO 的 ADC 原始值）

| ADC 范围 | 雨情 |
|---|---|
| **> 2500** | 无雨 |
| **2500 ~ 2000** | 小雨 |
| **2000 ~ 1500** | 中雨 |
| **≤ 1500** | 大雨 |

阈值定义在 `user/api/common.h`：
```c
#define RAIN_ADC_THRESHOLD_SMALL  (2500)
#define RAIN_ADC_THRESHOLD_MIDDLE (2000)
#define RAIN_ADC_THRESHOLD_BIG    (1500)
```

### 水位换算（非线性门限表 + 噪声死区）

水位传感器输出与液位**非线性**，不能线性换算。使用门限表逐段判定：

| ADC 门限 | 液位 | 高度 |
|---|---|---|
| ≥1100 | 100% | 90mm |
| ≥900 | 90% | 81mm |
| ≥600 | 80% | 72mm |
| ≥440 | 70% | 63mm |
| ≥340 | 60% | 54mm |
| ≥300 | 50% | 45mm |
| ≥250 | 40% | 36mm |
| ≥220 | 30% | 27mm |
| ≥190 | 20% | 18mm |
| ≥160 | 10% | 9mm |
| <160 | 0% | 0mm |

**噪声死区**：`height ≤ 9mm`（即 ADC 160~189，原 10% 档）会被强制归零。

> **为什么加死区**：传感器干燥时 ADC 会在 160~189 之间抖动，落进 10% 档，
> 而 `10% × 90mm = 9mm`，导致"没水也报 9mm"。加了死区后该档直接输出 0%，
> 彻底消除误报。宏定义在 `level.h` 的 `LEVEL_IGNORE_BELOW_MM`（默认 9）。

门限表在 `user/api/level.c` 的 `level_threshold[]` 数组。

> 水位传感器**只使用 AO 模拟量**，DO 数字量不接（详见"硬件接线"章节）。

### 水位门限实测校准

如果满水时显示不到 100%，说明门限表和实际传感器量程不匹配。
串口输出里的 `(ADC=xxx)` 就是校准依据：

1. 空水时记录 ADC 值（如 20）
2. 满水时记录 ADC 值（如 716）
3. 按 `门限_i = 空值 + (满值 - 空值) × 百分比` 重算门限表

---

## 六、源码结构

```
STM32F103ZET6_my_loge/
├── libraries/              CMSIS + STM32F10x 标准外设库（原始，未改）
├── project/
│   ├── STM32F103ZET6_Project.uvprojx    Keil 工程
│   └── Objects/STM32F103ZET6_Project.hex  编译产物
└── user/
    ├── main.c              主程序：初始化 + 主循环
    ├── stm32f10x_it.c      中断向量（原始）
    └── api/
        ├── common.h        ★ 所有引脚/阈值宏配置集中在这里
        ├── sensor_report.c ★ 采集调度 + 组帧 + LoRa 发送 + 中文打印
        ├── sensor_report.h ★ REPORT_INTERVAL_MS 在这里
        ├── raindrop.c      雨滴传感器驱动
        ├── raindrop.h
        ├── level.c         水位传感器驱动
        ├── level.h         ★ LEVEL_IGNORE_BELOW_MM 在这里
        ├── dht20.c         DHT20 温湿度驱动（软件 I2C）
        ├── dht20.h
        ├── lora_as32.c     AS32 LoRa 模块驱动
        ├── lora_as32.h
        ├── usart1.c        调试串口（DMA 发送 + IDLE 中断接收）
        ├── usart1.h
        ├── usart3.c        LoRa 串口（中断收发）
        ├── usart3.h
        ├── delay.c         软件延时
        ├── led.c / beep.c  板载 LED / 蜂鸣器
        ├── i2c.c           硬件 I2C 驱动（当前未使用）
        └── oled_font.c     OLED 字库（从机未使用）
```

### 核心模块说明

| 文件 | 职责 |
|---|---|
| `sensor_report.c` | 采集三路传感器 → 组 ASCII 帧 → LoRa 发送 → 本地中文打印 |
| `raindrop.c` | 雨滴 DO 消抖 + AO 均值滤波 + ADC 值分档 |
| `level.c` | 水位 AO 均值滤波 + 非线性门限换算 + 噪声死区 |
| `dht20.c` | DHT20 软件 I2C 驱动，含总线恢复 |
| `lora_as32.c` | LoRa 配置读写、模式切换、透明传输发送 |

---

## 七、关键技术点（改动时注意）

### 1. ADC1 被两个传感器共用

- 雨滴 AO = **PC2 = ADC1_IN12**
- 水位 AO = **PA1 = ADC1_IN1**

**每次采样前必须重新配置规则通道**（`SQR3`），否则会读到另一个通道的值。两个模块都在各自的 `ReadAdc()` 里做了这件事。

### 2. ADC 采样用寄存器直操

`raindrop.c` 和 `level.c` 的 ADC 采样都直接操作寄存器，不用标准库封装：

```c
ADC1->SMPR1 = (7UL << 6);      // 采样时间
ADC1->SQR1 = 0;                 // 序列长度 1
ADC1->SQR3 = 通道号;
ADC1->CR2 |= (1UL << 0);        // ADON 唤醒
ADC1->CR2 = ADON|EXTTRIG|EXTSEL=7|SWSTART;  // 启动
while (!(ADC1->SR & EOC));      // 等完成
result = ADC1->DR & 0x0FFF;
```

**⚠️ 绝对不要在采样函数里做 `RCC_APB2PeriphResetCmd(ADC1, ...)` 复位**，那会把整个 ADC 外设重置，寄存器全部归零，转换永远无法完成。

### 3. 串口收发有竞态保护

`usart1.h` / `usart3.h` 提供了 `USARTx_TakeFrame()` 函数：

```c
if (USART3_TakeFrame(buf, &len)) { ... }
```

内部是「关中断 → 拷贝 → 清空 → 开中断」，避免主循环 `memset` 时把中断刚收到的数据清掉。

**不要改回**「先判断 flag 再 memset 全局结构体」的写法。

### 4. LoRa 双端参数必须一致

| 参数 | 当前值 |
|---|---|
| 地址 | 0x0000 |
| 信道 | 0x3D（471MHz） |
| 串口 | 9600 8N1 |
| 空中速率 | 2.4k |
| 模式 | 透明传输（OPTION=0x40） |

**收发双方模块的地址、信道、空中速率必须完全一致**，否则收不到。

> 注意：AS32-TTL-100-C 出厂信道是 **0x3D（471MHz）**，而 AS32-TTL-100 是 **0x17（433MHz）**，两者不通用。

### 5. 栈只有 1KB

启动文件 `startup_stm32f10x_hd.s` 里 `Stack_Size = 0x400`（1KB）。

`main.c` 的接收缓冲区放在**静态区**（`static u8 s_rxBuf[]`），不放栈上，避免栈溢出。

### 6. 文件编码是混合的（重要）

**不同文件编码不一样，修改时必须逐个确认**：

| 文件 | 编码 |
|---|---|
| `sensor_report.c` / `sensor_report.h` | **GBK** |
| `level.c` / `level.h` | **UTF-8 与 GBK 混合** |
| 其他 `.c` / `.h` | 多为 GBK |

`level.c` / `level.h` 里混了两种编码，用单一编码打开会乱码。
**改动时只改纯 ASCII 的代码区，中文注释一个字节都别碰**，否则整份文件会毁掉。

串口助手也要选 **GBK** 才能正常显示中文。

### 7. 上报周期改短后要注意耗时

当前 `REPORT_INTERVAL_MS = 1000`（1 秒）。单次 `Report_Once()` 耗时构成：

| 环节 | 耗时 |
|---|---|
| 雨滴 AO 采样（10 次 + 5 次自检） | ~1ms |
| 水位采样（10 次 + 5 次自检） | ~2ms |
| DHT20 单次测量 | ~100ms |
| LoRa 发送 71 字节 @9600 | ~74ms |
| **合计** | **~200~250ms** |

1 秒周期余量约 750ms，安全。**不要低于 500ms**，否则 DHT20 会来不及。

---

## 八、编译与烧录

### 编译

Keil 打开 `project/STM32F103ZET6_Project.uvprojx`，按 **F7**。

或用命令行：
```powershell
& 'C:\Keil_v5\UV4\UV4.exe' -b 'project/STM32F103ZET6_Project.uvprojx' -j0 -o build.log
```

### 烧录

Keil 按 **F8**，或直接烧 `project/Objects/STM32F103ZET6_Project.hex`。

**⚠️ 重要：如果 Keil 里打开着某个文件，改完代码后不要再保存那个文件**，否则会用编辑器里的旧内容覆盖磁盘上的新版本。改完执行 **File → Reload**。

---

## 九、故障排查

| 现象 | 检查 |
|---|---|
| `LORA CONFIG READ FAIL` | ① PB10/PB11 是否接反（模块 TXD→PB11，RXD→PB10）② **P5 跳线帽**是否选到无线模块 ③ 模块供电 |
| 对端收不到 | 两端 LoRa 的**信道、空中速率、地址**必须完全一致 |
| `温度:--` | DHT20 的 VCC/GND/SCL(PB6)/SDA(PB7)；SCL/SDA 是否接 4.7K~10K 上拉 |
| 温度偏高 | DHT20 靠近 MCU 会被芯片发热影响，装远一点 |
| `水位:--` | ① PA1 是否接了水位模块 AO ② **P10 跳线帽必须拔掉**（否则 PA1 被 TPAD 占用）③ 模块是否 5V 供电 |
| 水位读数乱跳 | 拔 P10 跳线帽（PA1 兼作 TPAD 触摸脚） |
| **没水也报 9mm** | 已通过 `LEVEL_IGNORE_BELOW_MM` 修复；若仍有，把该宏调大（如 18） |
| **满水只显示 60%** | 门限表与传感器量程不匹配，用串口 `(ADC=xxx)` 重新校准门限表 |
| 雨情一直"无雨" | 滴一滴水在感应板，看 ADC 是否下降（干燥时 ADC 约 2900~3500，判无雨是正常的） |
| 串口中文乱码 | 串口助手编码选 **GBK** |

---

## 十、可调参数速查

| 参数 | 位置 | 当前值 |
|---|---|---|
| **上报周期** | `sensor_report.h` → `REPORT_INTERVAL_MS` | **1000**（1 秒） |
| 雨情阈值 | `common.h` → `RAIN_ADC_THRESHOLD_*` | 2500 / 2000 / 1500 |
| 雨滴 DO 引脚 | `common.h` → `RAIN_DO_GPIO_PIN` | `GPIO_Pin_1`（PC1） |
| 雨滴 AO 引脚 | `common.h` → `RAIN_AO_GPIO_PIN` | `GPIO_Pin_2`（PC2） |
| 雨滴 ADC 通道 | `common.h` → `RAIN_AO_ADC_CHANNEL` | `ADC_Channel_12` |
| 雨滴滤波次数 | `common.h` → `RAIN_FILTER_TIMES` | 10 |
| 水位 AO 通道 | `level.h` → `LEVEL_ADC_CHANNEL` | `ADC_Channel_1`（PA1） |
| 水位门限表 | `level.c` → `level_threshold[]` | 1100/900/600/.../160 |
| **水位噪声死区** | `level.h` → `LEVEL_IGNORE_BELOW_MM` | **9**（mm） |
| 水位量程 | `level.h` → `LEVEL_RANGE_MM` | 90（mm） |
| 水位滤波次数 | `level.h` → `LEVEL_SAMPLE_TIMES` | 10 |
| LoRa 信道 | `lora_as32.h` → `LORA_DEFAULT_CHAN` | 0x3D |
| LoRa 空中速率 | `main.c` → `LORA_AIR_SPEED_2K4` | 2.4k |
| 调试串口波特率 | `main.c` → `USART1_BRR` | 9600 |

---

## 十一、本工程修复过的历史 Bug（供参考，勿回退）

| Bug | 位置 | 问题 |
|---|---|---|
| ADC 复位 | `raindrop.c` | 采样函数里做了 ADC 外设复位，导致寄存器归零、转换永不完成 |
| ISR memset | `usart1.c`/`usart3.c` | 中断开头 `memset` 会吞掉整帧，已改为消费者清空 |
| 缓冲区越界 | `usart1.c`/`usart3.c` | 接收满 256 字节后继续写会越界，已加边界判断 |
| DHT20 校准 | `dht20.c` | `0xFF & 0x18 == 0x18` 导致"读失败"被误判为"已校准"，校准被永久跳过 |
| ADC 死等 | `raindrop.c` | `while(EOC==RESET);` 无超时，ADC 异常时永久卡死 |
| 消抖逻辑 | `raindrop.c` | 一抖动就提前退出，3 次实际只比 1 次 |
| `strstr` 越界 | `lora_as32.c` | 对非 NUL 结尾的 u8 缓冲做 strstr，已改为带长度查找 |
| Keil 工程路径 | `.uvprojx` | 登记的文件名是 `raindrop.c.c`，实际是 `raindrop.c`，导致工程编译不过 |
| `SYSTEM_CORE_CLOCK` | `raindrop.c` | 宏未定义，已改用 CMSIS 的 `SystemCoreClock` |
| **水位 9mm 误报** | `level.c` / `level.h` | 干燥噪声落进 10% 档 → 9mm。已加 `LEVEL_IGNORE_BELOW_MM` 死区 |

---

## 十二、当前状态与待办

### 已验证

- ✅ 编译 0 Error
- ✅ 雨滴传感器工作正常（实测 ADC 2895~3500，滴水后明显下降）
- ✅ 水位传感器工作正常（实测空水 ADC≈20，满水 ADC≈716，满水显示 80%）
- ✅ DHT20 工作正常（实测 26.4℃ / 49.6%RH）
- ✅ LoRa 单机发送正常（串口每秒输出一行）
- ✅ 9mm 误报已修复
- ✅ 水位百分比显示正常

### 待硬件验证

- ⬜ LoRa 双机通信联调（需与主机工程配合）
- ⬜ 长时间稳定性测试

### 已知硬件问题

- 旧板子的 **ADC1 损坏**（所有通道恒读 4095，含内部 Vrefint），已换板解决

---

## 十三、配套工程

| 工程 | 说明 |
|---|---|
| [`../master`](../master/) | **主机（接收端）**：接收本机数据，OLED 显示 + 水位报警 |
| DHT20 温湿度传感器参考例程 | 移植来源之一 |
| 水位传感器（XPT2046 液位板）参考例程 | 移植来源之一 |
| 雨滴传感器参考例程 | 移植来源之一 |

本工程的传感器驱动均从上述工程移植并适配到当前引脚配置。

---

## 十四、与主机的协作要点

1. **两机 LoRa 参数必须完全一致**：地址 0x0000 / 信道 0x3D / 9600 8N1 / 2.4k / 透明传输
2. **从机每秒发一帧，主机每 10 秒刷屏一次** —— 数据实时，显示节流
3. **主机水位报警阈值 70%**（`WATER_ALARM_THRESHOLD`），超过立即蜂鸣 + 显示"水位过高"
4. 从机帧里的 `WL=` 字段是主机显示水位的唯一来源，**不要删除**
