#include "stm32f10x.h"
#include "common.h"

#include "delay.h"
#include "usart1.h"
#include "usart3.h"
#include "lora_as32.h"
#include "led.h"
#include "beep.h"
#include "sensor_report.h"

/*
================================================================================
【程序说明】雨滴传感器 + DHT20 温湿度 —— LoRa 无线上报
--------------------------------------------------------------------------------
	硬件连接
		USART1  PA9/PA10   接 USB 转 TTL 到 PC，用于本地打印调试（9600 8N1）
		USART3  PB10/PB11  接 AS32-TTL-100-C LoRa 模块（9600 8N1）
		        MD0 -> PB12   MD1 -> PB13   AUX -> PB14
		雨滴    DO  -> PC1（数字量，无水高电平、有水低电平）
		        AO  -> PC2（模拟量，接 ADC1_IN12）
		DHT20   SCL -> PB6   SDA -> PB7（软件模拟 I2C，与板载 AT24C02 共用总线）

	工作流程
		1. 初始化 USART1 / LoRa(USART3) / LED / 蜂鸣器
		2. 读一次 LoRa 模块配置与版本号，打印到 PC 端确认模块在线
		3. 把模块配置成：地址 0x0000、信道 0x3D(471MHz)、9600 8N1、
		   空中速率 2.4k、透明传输 —— 收发双方参数必须完全一致
		4. 初始化雨滴传感器与 DHT20
		5. 进入主循环：
		   5.1 每 10 秒采集一次雨滴 + DHT20，组帧后经 LoRa 发出，同时打印到 PC
		   5.2 对端 LoRa 发来的数据原样转发到 PC 串口助手（双向透传）
		   5.3 PC 串口助手输入的内容原样发给 LoRa（可手动和对端对话）
================================================================================
*/

#define USART1_BRR (9600)
#define LORA_BRR   (9600)

/* 主循环基础节拍（ms） */
#define LOOP_TICK_MS (10)

/* 单次上报的阻塞耗时估算（ms），用于补偿上报周期 */
#define REPORT_COST_ESTIMATE_MS (300)

/*
【为什么放静态区而不是栈上】
	启动文件 startup_stm32f10x_hd.s 里 Stack_Size 只有 0x400 = 1024 字节。
	DHT20 读一次要经过 DHT20_Read -> DHT20_Read_Raw -> 多个 I2C 时序函数，
	调用层级较深；若再在 main() 栈帧里放 256 字节接收缓冲区，
	叠加起来余量会很紧张，有栈溢出踩坏变量的风险。
	所以把收发缓冲区放到 .bss 静态区（不占栈），代价仅 256 字节 RAM。
*/
static u8 s_rxBuf[USART3_BUFFER_SIZE];

int main(void)
{
	LORA_Config config;
	char        version[32];
	uint32_t    tick = 0;
	u16         rxLen = 0;

	/* ---------------- 1. 硬件初始化 ---------------- */
	USART1_Init(USART1_BRR); /* USART1 -> PC 串口调试助手 */
	LORA_Init(LORA_BRR);     /* USART3 -> LoRa 模块，并拉低 MD0/MD1 进入一般工作状态 */
	Beep_Init();
	Led_Init();

	USART1_SendString("======== AS32-TTL-100-C LoRa + RainDrop + DHT20 ========\r\n");

	/* ---------------- 2. 读取模块当前配置，确认模块在线 ---------------- */
	if (LORA_ReadConfig(&config))
	{
		USART1_SendString("LORA CONFIG : ");
		USART1_SendBuffer((u8 *)&config, sizeof(LORA_Config));
		USART1_SendString("\r\n");
	}
	else
	{
		USART1_SendString("LORA CONFIG READ FAIL (check PB10/PB11 wiring and module power)\r\n");
	}

	/* ---------------- 3. 读取模块硬件版本号 ---------------- */
	if (LORA_ReadVersion(version, sizeof(version)))
	{
		USART1_SendString("LORA VERSION : ");
		USART1_SendString(version);
		USART1_SendString("\r\n");
	}
	else
	{
		USART1_SendString("LORA VERSION READ FAIL\r\n");
	}

	/*
	4. 写入模块配置
		【重点】通信双方模块的地址、信道、空中速率必须保持一致！
		出厂默认：C0 00 00 1A 3D 40
		若需要定点传输，把 option 改成 LORA_OPTION_FIXED
	*/
	config.addh   = 0x00;
	config.addl   = 0x00;
	config.speed  = LORA_UART_8N1 | LORA_UART_BAUD_9600 | LORA_AIR_SPEED_2K4;
	config.chan   = LORA_DEFAULT_CHAN;
	config.option = LORA_OPTION_TRANSPARENT;

	if (LORA_SetConfig(&config))
	{
		USART1_SendString("LORA CONFIG SET OK\r\n");
	}
	else
	{
		USART1_SendString("LORA CONFIG SET FAIL (module may be offline, still trying to report)\r\n");
	}

	/* ---------------- 5. 清掉配置阶段残留的应答数据 ---------------- */
	memset(&usart3_value, 0, sizeof(USART3_Data));
	memset(&usart1_value, 0, sizeof(USART1_Data));

	/* ---------------- 6. 传感器初始化 ---------------- */
	Report_Init();

	USART1_SendString("Start sampling, report via LoRa every 10 seconds\r\n\r\n");

	/* ---------------- 7. 主循环 ---------------- */
	while (1)
	{
		/*
		7.1 周期上报：每 REPORT_INTERVAL_MS 采集一次并通过 LoRa 发出
			用计数器而不是 delay_ms 整段阻塞，
			这样等待期间还能及时把 LoRa 收到的数据转发给 PC
		*/
		if (tick >= REPORT_INTERVAL_MS)
		{
			/*
			 * [FIX-DBG4] 周期计时修正
			 *   原写法 tick = 0 有两个问题：
			 *     (1) 丢掉溢出的那一点时间，长期累积偏慢；
			 *     (2) Report_Once() 阻塞的约 300ms 完全不计入 tick，
			 *         导致实际周期 = 10 秒 + 上报耗时。
			 *   改为：
			 *     - tick -= REPORT_INTERVAL_MS 保留溢出余量；
			 *     - tick += REPORT_COST_ESTIMATE_MS 把上报阻塞时间补进计时，
			 *       使周期稳定在 10 秒附近（仿真验证：偏差由 11% 降到约 8%，
			 *       剩余偏差来自 delay_ms 软件循环本身的误差）。
			 */
			tick -= REPORT_INTERVAL_MS;

			Report_Once();
			Led0_Toggle; /* 每上报一次，LED0 翻转一次，直观看到程序在跑 */

			/* 把本次上报阻塞的时间补进计时，避免周期被拉长 */
			tick += REPORT_COST_ESTIMATE_MS;
		}

		/*
		7.2 LoRa -> PC
			对端模块发来的数据，原样送到本机串口助手
			【修复竞态】不能用 "先判断 flag 再 memset 全局结构体" 的写法：
			主循环在 USART1_SendBuffer() 里会阻塞几十毫秒，这期间 USART3 中断
			可能已经收完新一帧并写入 usart3_value，此时主循环再 memset 就把新帧清掉了。
			改用 USART3_TakeFrame() 函数：关中断 -> 整体拷出 -> 清空 -> 开中断，不会丢帧。
		*/
		if (USART3_TakeFrame(s_rxBuf, &rxLen))
		{
			if (rxLen > 0)
			{
				USART1_SendBuffer(s_rxBuf, rxLen);
				Beep_Toggle; /* 收到一帧，蜂鸣器翻转一次 */
			}
		}

		/*
		7.3 PC -> LoRa
			串口助手里输入的内容，原样发给对端模块
			同样用 TakeFrame 避免竞态丢数据
		*/
		if (USART1_TakeFrame(s_rxBuf, &rxLen))
		{
			if (rxLen > 0)
			{
				LORA_SendData(s_rxBuf, rxLen);
			}
		}

		/*
		 * 7.4 轮询节拍
		 *   注意：delay_ms() 是纯软件循环，-O0 下实际约 10.8ms，但这里按标称
		 *   10ms 计入 tick，属于已知的轻微偏差（约 8%）。要完全精确需要改用
		 *   硬件定时器，当前 10 秒周期下影响可接受。
		 */
		delay_ms(LOOP_TICK_MS);
		tick += LOOP_TICK_MS;
	}
}
