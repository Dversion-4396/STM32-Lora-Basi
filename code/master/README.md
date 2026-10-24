# STM32F103ZET6 LoRa 主机工程 —— 接收端 + OLED 显示 + 水位报警

> 最后更新：2026-10-24
> 编译状态：**0 Error / 3 Warning**（3 个警告均来自 `i2c.c` 的整数溢出，是原工程遗留，不影响功能）
> 固件：`project/Objects/STM32F103ZET6_Project.hex`（373712 字节，含 120KB OLED 字库）
> 角色：**主机（接收端）** —— 接收从机 LoRa 数据，OLED 显示，水位超限报警

---

## 一、工程功能

接收从机通过 LoRa 发来的传感器数据帧，解析后显示在 **0.96 寸 SSD1306 OLED**（128×64）上，并在水位超限时蜂鸣报警。

```
              LoRa 天线
                 ▲
                 │
   从机 ──► USART3(PB10/PB11) ──► 帧解析 ──► OLED(PB6/PB7 软件 I2C)
                                    │
                                    ├──► USART1(PA9/PA10) ──► PC 串口助手
                                    │
                                    └──► 蜂鸣器(PB8) 水位超限报警
```

**核心特性**：
- 数据**实时**（从机每秒一帧，随时更新到内存）
- 屏幕**每 10 秒**刷新一次（避免 OLED 频繁刷新闪烁）
- 水位 **> 70%** 时**立即**蜂鸣 + 立即显示"水位过高"（**不遵循** 10 秒刷新规则）

---

## 二、硬件接线

### 引脚分配

| 功能 | 引脚 | 外设/模式 | 备注 |
|---|---|---|---|
| **OLED SCL** | **PB6** | 开漏输出 | 软件 I2C，需 4.7K~10K 上拉 |
| **OLED SDA** | **PB7** | 开漏/浮空 | 软件 I2C，需 4.7K~10K 上拉 |
| LoRa TXD→ | PB11 | USART3_RX | 模块 TXD 接单片机 RX |
| LoRa RXD← | PB10 | USART3_TX | 模块 RXD 接单片机 TX |
| LoRa MD0 | PB12 | 开漏输出 | 模式选择低位 |
| LoRa MD1 | PB13 | 开漏输出 | 模式选择高位 |
| LoRa AUX | PB14 | 浮空输入 | 模块忙/闲状态 |
| 调试串口 | PA9/PA10 | USART1 | 9600 8N1，接 PC |
| LED0 / LED1 | PB5 / PE5 | 推挽输出 | 低电平点亮 |
| **蜂鸣器** | **PB8** | 推挽输出 | 水位超限报警 |

> **注意**：本机 **PB6/PB7 接 OLED**，从机 PB6/PB7 接 DHT20。
> 两台板子各自独立，不冲突。

### OLED 接线（0.96" SSD1306，4 线 I2C）

```
VCC → 3.3V
GND → GND
SCL → PB6   ← 必须 4.7K~10K 上拉到 3.3V
SDA → PB7   ← 必须 4.7K~10K 上拉到 3.3V
```

I2C 从机地址：**0x3C**（模块标称 0x78 是 8 位写法，`0x78 >> 1 = 0x3C`）

### LoRa 模块（AS32-TTL-100-C）

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

## 三、OLED 显示格式

屏幕分 4 行（每行占 2 页 = 16px 高）：

### 正常显示

```
温度:26.5
湿度:52.2
水位:80%
无雨
```

第 4 行根据雨情等级显示：`无雨` / `小雨` / `中雨` / `大雨`

### 报警显示（水位 > 70%）

```
温度:26.5
湿度:52.2
水位:75%!
水位过高
```

- 第 3 行水位后加 `!` 强调
- 第 4 行被"水位过高"占用（雨情不显示，保证报警最醒目）
- 蜂鸣器每 500ms 翻转，发出"哔—哔—"间隔报警声

### 等待显示（上电 / 断线 5 秒后）

```
等待从机
LoRa 数据...
```

### 无效数据显示

```
温度:--
湿度:--
水位:--
无雨
```

DHT20 失败时温湿度是 `--`；从机是老固件（无 `WL` 字段）时水位是 `--`。

---

## 四、接收帧格式

从机每秒发来的 ASCII 文本帧：

```
$RAIN,DHT20,T=26.5,H=52.2,ADC=4092,V=3297,WET=0,DO=1,LEVEL=0,WL=80*8B\r\n
```

| 字段 | 含义 | 本机用途 |
|---|---|---|
| `$RAIN,DHT20,` | 固定帧头 | 校验，不匹配则丢弃 |
| `T=` | 温度 ℃ | OLED 第 1 行 |
| `H=` | 湿度 %RH | OLED 第 2 行 |
| `ADC=` | 雨滴 ADC 原始值 | 仅解析，不显示 |
| `V=` | 雨滴电压 mV | 仅解析，不显示 |
| `WET=` | 雨滴湿润百分比 | 仅解析，不显示 |
| `DO=` | 雨滴数字量 | 仅解析，不显示 |
| `LEVEL=` | 雨情等级 0~3 | OLED 第 4 行 |
| `WL=` | **水位百分比 0~100** | OLED 第 3 行 + 报警判定 |
| `*XX` | 校验和 | **必须验证**，不匹配则丢弃 |
| `\r\n` | 帧尾 | 拼帧边界 |

---

## 五、关键技术点（改动时注意）

### 1. ⚠️ LoRa 模块会把一帧拆成 32 字节碎片

**这是本工程最关键的坑**，也是最容易踩的。

AS32-TTL-100-C 通过空口收到一包数据后，**不是一次性吐到串口**，而是按 **32 字节一块**分块输出。块与块之间串口空闲 ≥1 字符时间，会触发 IDLE 中断。

实测日志（71 字节的一帧被拆成 3 段）：

```
[RX] $RAIN,DHT20,T=26.5,H=52.2,A     ← 32 字节
DC=4092,V=3297,WET=0,DO=1,LEVEL      ← 32 字节
=0,WL=80*8B                           ← 7 字节
```

**如果每次 IDLE 收到的数据就当一帧去解析，必然失败** —— 找不到完整帧头和校验和，`master_data` 永远不更新，屏幕就显示 `温度:--`。

**解决方案**：`frame_assembler.c` 流式拼帧器

```c
while (FrameAsm_Push(&s_frameAsm, s_rxBuf, rxLen,
                     s_frameBuf, sizeof(s_frameBuf), &frameLen))
{
    if (Master_ParseFrame(s_frameBuf, frameLen)) { ... }
    rxLen = 0;   // 后续碎片已在缓冲里，继续扫
}
```

拼帧器做三件事：
1. 碎片追加到缓冲，扫描 `\r\n` 找帧边界
2. 行首必须是 `$`，否则是残留垃圾，整行丢弃
3. 缓冲里一个 `$` 都没有 → 判定全是垃圾，清空重新同步

### 2. 屏幕刷新与报警的优先级

```c
/* 报警跳变时：立即刷屏 + 立即蜂鸣器（不遵循 10 秒周期） */
if (master_data.waterAlarm != prevAlarm)
{
    prevAlarm = master_data.waterAlarm;
    if (master_data.waterAlarm) { beepOn = 1; Beep_Ctrl(1); }
    else                        { beepOn = 0; Beep_Ctrl(0); }
    if (!showingWait) { screenMs = 0; Master_Display(); }
}

/* 常规 10 秒周期刷屏 */
if ((screenMs >= SCREEN_REFRESH_MS) && screenDirty && (!showingWait))
{
    screenMs = 0; screenDirty = 0; Master_Display();
}
```

- 数据每秒更新到 `master_data`（实时）
- 屏幕只在 10 秒周期点渲染（节流，防闪烁）
- **报警是例外**：状态跳变立即刷屏，安全优先

### 3. 串口 DMA 只有一个发送缓冲

`USART1_SendBuffer()` 内部会先 `USART1_WaitTxComplete()` 等上一帧真正发完。

**不能连续调用多次**：

```c
// ❌ 错误：3 次调用白等两次，9600bps 下约 150ms
USART1_SendString("[RX] ");
USART1_SendBuffer(s_frameBuf, frameLen);
USART1_SendString("\r\n");

// ✅ 正确：先拼成一个缓冲，一次发出
USART1_SendBuffer(s_txLine, k);
```

同时 `s_txLine` 容量必须 ≤ `USART1_TX_BUFFER_SIZE`（256），超长会被**静默截断**。

### 4. OLED 字库必须开启

`oled_font.h` 里：

```c
#define OLED_USE_FONT   1    // 必须是 1，否则汉字显示不出来
```

设为 1 后字库参与编译，Flash 增加约 120KB（`RO-data` 从 498 → 122206）。STM32F103ZE 有 512KB Flash，够用。

字库覆盖 **GB2312 一级汉字**（3755 字），`温度/湿度/水位/无雨/小雨/中雨/大雨/等待从机` 都在范围内。

### 5. 文件编码是 GBK（重要）

**所有 `.c` / `.h` 必须是 GBK 编码**，因为 `OLED_ShowString()` 按 GBK 解码汉字。UTF-8 编码的中文字面量会显示成乱码。

修改时注意：
- 用支持 GBK 的编辑器
- 或改完用 Python 转码：`open(p,'wb').write(s.encode('gbk'))`

### 6. LoRa 双端参数必须一致

| 参数 | 当前值 |
|---|---|
| 地址 | 0x0000 |
| 信道 | 0x3D（471MHz） |
| 串口 | 9600 8N1 |
| 空中速率 | 2.4k |
| 模式 | 透明传输（OPTION=0x40） |

**必须和从机完全一致**，否则收不到。

### 7. 栈只有 1KB

启动文件 `Stack_Size = 0x400`。接收缓冲区 `s_rxBuf`、拼帧缓冲 `s_frameAsm`、发送缓冲 `s_txLine` 全部放**静态区**，不放栈上。

### 8. 断线必须清报警

超时 5 秒没收到数据时，除了显示"等待从机"，还必须：

```c
master_data.waterAlarm = 0;
prevAlarm = 0;
beepOn = 0;
Beep_Ctrl(0);
```

否则断线前正在报警的话，蜂鸣器会**无限期长响**。

---

## 六、源码结构

```
STM32F103ZET6_my_lora_master/
├── libraries/              CMSIS + STM32F10x 标准外设库（原始，未改）
├── project/
│   ├── STM32F103ZET6_Project.uvprojx    Keil 工程
│   └── Objects/STM32F103ZET6_Project.hex  编译产物
└── user/
    ├── main.c              ★ 主程序：接收 + 拼帧 + 解析 + 刷新调度 + 报警
    ├── stm32f10x_it.c      中断向量（原始）
    └── api/
        ├── common.h        引脚/阈值宏配置
        ├── master_app.c    ★ 帧解析 + OLED 显示
        ├── master_app.h    ★ 报警阈值 / 刷新周期宏
        ├── frame_assembler.c  ★ LoRa 流式拼帧器（关键）
        ├── frame_assembler.h
        ├── oled.c          SSD1306 OLED 驱动
        ├── oled.h
        ├── oled_font.c     GB2312 一级汉字字库（120KB）
        ├── oled_font.h     ★ OLED_USE_FONT 开关
        ├── i2c.c           软件 I2C（驱动 OLED）
        ├── i2c.h           ★ OLED_ADDR / WRITE_FLAG 等宏
        ├── lora_as32.c     AS32 LoRa 模块驱动
        ├── lora_as32.h
        ├── usart1.c        调试串口（DMA 发送）
        ├── usart1.h
        ├── usart3.c        LoRa 串口（中断收发）
        ├── usart3.h
        ├── delay.c         软件延时
        ├── led.c / beep.c  板载 LED / 蜂鸣器
        ├── sensor_report.c 传感器采集（本机不使用，保留）
        ├── raindrop.c / level.c / dht20.c  （本机不使用，保留）
        └── ...
```

### 核心模块说明

| 文件 | 职责 |
|---|---|
| `main.c` | 主循环：收帧 → 拼帧 → 解析 → 10 秒刷屏调度 → 报警处理 |
| `master_app.c` | `Master_ParseFrame()` 帧解析、`Master_Display()` OLED 渲染 |
| `frame_assembler.c` | 把 LoRa 的 32 字节碎片拼成完整帧 |
| `oled.c` | SSD1306 驱动，支持 GBK 汉字显示 |
| `i2c.c` | PB6/PB7 软件 I2C，驱动 OLED |

---

## 七、编译与烧录

### 编译

Keil 打开 `project/STM32F103ZET6_Project.uvprojx`，按 **F7**。

或用命令行：
```powershell
& 'C:\Keil_v5\UV4\UV4.exe' -b 'project/STM32F103ZET6_Project.uvprojx' -j0 -o build.log
```

### 烧录

Keil 按 **F8**，或直接烧 `project/Objects/STM32F103ZET6_Project.hex`。

**⚠️ 如果 Keil 里打开着某个文件，改完代码后不要再保存那个文件**，否则会用编辑器里的旧内容覆盖磁盘上的新版本。改完执行 **File → Reload**。

---

## 八、故障排查

| 现象 | 检查 |
|---|---|
| **OLED 完全不亮** | ① PB6/PB7 接线 ② SCL/SDA 是否接 4.7K~10K 上拉 ③ VCC 是否 3.3V ④ 地址是否 0x3C |
| **OLED 显示方块/乱码** | `oled_font.h` 的 `OLED_USE_FONT` 是否为 1；`.c` 文件是否为 GBK 编码 |
| **屏幕一直"等待从机"** | 从机没发数据，或 LoRa 参数不一致，或接收中断没工作 |
| **屏幕显示 温度:--** | 收到帧但解析失败。看串口 `[RX]` 行是否完整 71 字节；有 `[WARN]` 说明校验失败 |
| **串口 `[RX]` 只有 32 字节碎片** | 拼帧器没生效，检查 `frame_assembler.c` 是否注册进 Keil 工程 |
| **水位一直显示 --** | 从机固件太老，帧里没有 `WL=` 字段，需重新烧从机 |
| **报警后蜂鸣器一直响** | 断线未清报警，检查超时分支是否清了 `waterAlarm` / `Beep_Ctrl(0)` |
| **水位 70% 不报警，71% 才报** | 正常，代码是严格"大于 70" |
| `LORA CONFIG READ FAIL` | PB10/PB11 是否接反（模块 TXD→PB11，RXD→PB10）；模块供电 |
| 串口中文乱码 | 串口助手编码选 **GBK** |

---

## 九、可调参数速查

| 参数 | 位置 | 当前值 |
|---|---|---|
| **水位报警阈值** | `master_app.h` → `WATER_ALARM_THRESHOLD` | **70**（%） |
| **屏幕刷新周期** | `master_app.h` → `OLED_REFRESH_MS` | **10000**（10 秒） |
| 断线超时 | `main.c` → `RX_TIMEOUT_MS` | 5000（5 秒） |
| 主循环节拍 | `main.c` → `LOOP_TICK_MS` | 10（ms） |
| 拼帧缓冲容量 | `frame_assembler.h` → `FRAME_ASM_BUF_SIZE` | 256 |
| 串口发送缓冲 | `main.c` → `TX_LINE_MAX` | 240（≤ DMA 上限 256） |
| OLED 字库开关 | `oled_font.h` → `OLED_USE_FONT` | **1**（必须） |
| OLED I2C 地址 | `i2c.h` → `OLED_ADDR` | 0x3C |
| LoRa 信道 | `lora_as32.h` → `LORA_DEFAULT_CHAN` | 0x3D |
| LoRa 空中速率 | `main.c` → `LORA_AIR_SPEED_2K4` | 2.4k |
| 调试串口波特率 | `main.c` → `USART1_BRR` | 9600 |

### 常用调整

**想让报警更早触发**：
```c
// master_app.h
#define WATER_ALARM_THRESHOLD  (60)   // 改成 60
```

**想让屏幕刷新更快**：
```c
// master_app.h
#define OLED_REFRESH_MS        (5000)  // 改成 5 秒
```

**想改报警蜂鸣节奏**：
```c
// main.c 搜 500
if (beepMs >= 500)   // 改小 = 响得更急，改大 = 响得更慢
```

---

## 十、本工程修复过的历史 Bug（供参考，勿回退）

| Bug | 位置 | 问题 |
|---|---|---|
| **帧被拆成 32 字节碎片** | `frame_assembler.c` | LoRa 模块分块吐数据，每次 IDLE 当一帧解析必然失败 → 屏幕显示 `温度:--`。已加流式拼帧 |
| **拼帧器垃圾堵塞** | `frame_assembler.c` | 缓冲残留垃圾时永远等不到换行，堵死后续正常帧。已改为"无 `$` 即清空" |
| **断线蜂鸣器长响** | `main.c` | 断线后 `waterAlarm` 保持旧值，报警中会无限响。已在超时分支清报警 |
| **串口 3 次调用白等 150ms** | `main.c` | `USART1_SendBuffer` 只有一个 DMA 缓冲，连续调用会互相等待。已改为拼一次发出 |
| **`s_txLine` 超 DMA 上限** | `main.c` | 272 > 256 会被静默截断。已缩到 240 并加双重检查 |
| **报警要等 20 秒才显示** | `main.c` | 只标脏不立即刷屏。已改为跳变触发立即刷屏 |
| **`oled.h` 引用不存在的 `usart.h`** | `oled.h` | 移植残留，OLED 不用串口。已删除 |
| **缺 `OLED_ADDR` 等宏** | `i2c.h` | 原本在 `usart.h` 里随文件丢失。已补进 `i2c.h` |
| **`OLED_USE_FONT = 0`** | `oled_font.h` | 字库不参与编译，汉字显示不出来。已改 1 |
| **`uvprojx` 插错分组** | `.uvprojx` | 新文件插进库分组破坏 XML 嵌套，导致不参与编译。已修正并验证 XML 合法 |
| **`rxLen` 类型不匹配** | `main.c` | 声明 `u32` 但 `TakeFrame` 要 `u16*`。已改 `u16` |

---

## 十一、当前状态

### 已验证

- ✅ 编译 0 Error / 3 Warning
- ✅ OLED 初始化正常，汉字显示正常（字库已启用）
- ✅ 帧解析逻辑离线仿真 8 项全通过
- ✅ 拼帧器单元测试 6 项全通过（含真实 32+32+7 碎片）
- ✅ 端到端仿真通过（碎片 → 拼帧 → 解析 → 显示）
- ✅ 刷新时序仿真通过（10 秒周期 + 报警立即响应）
- ✅ 报警边界验证（70 不报 / 71 报警）
- ✅ 断线停响验证

### 待硬件验证

- ⬜ 实机 OLED 显示效果
- ⬜ 实机报警蜂鸣器效果
- ⬜ 双机联调长时间稳定性

---

## 十二、与从机的协作要点

| 项目 | 从机 | 主机 |
|---|---|---|
| 角色 | 发送端 | 接收端 |
| 上报周期 | **1 秒** | — |
| 屏幕刷新 | — | **10 秒** |
| 水位报警 | — | **> 70% 立即** |
| LoRa 参数 | 0x0000 / 0x3D / 9600 / 2.4k / 透明 | **必须完全相同** |
| PB6/PB7 | DHT20 | **OLED** |
| PB8 | 蜂鸣器 | **蜂鸣器（报警）** |

**必须两机都烧对应固件**，且 LoRa 参数完全一致。

从机帧里的 `WL=` 字段是主机显示水位的唯一来源，**从机不要删除该字段**。

---

## 十三、配套工程

| 工程 | 说明 |
|---|---|
| [`../slave`](../slave/) | **从机（发送端）**：采集传感器 + LoRa 发送 |
| DHT20 温湿度传感器参考例程 | 移植来源之一 |
| 水位传感器（XPT2046 液位板）参考例程 | 移植来源之一 |
| 雨滴传感器参考例程 | 移植来源之一 |
