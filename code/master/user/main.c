#include "stm32f10x.h"
#include "common.h"

#include "delay.h"
#include "usart1.h"
#include "usart3.h"
#include "lora_as32.h"
#include "led.h"
#include "beep.h"
#include "master_app.h"
#include "frame_assembler.h"

/*
================================================================================
【程序说明】LoRa 主机 —— 接收从机数据并用 OLED 显示
--------------------------------------------------------------------------------
	硬件连接
		USART1  PA9/PA10   接 USB 转 TTL 到 PC，用于本地打印调试（9600 8N1）
		USART3  PB10/PB11  接 AS32-TTL-100-C LoRa 模块（9600 8N1）
		        MD0 -> PB12   MD1 -> PB13   AUX -> PB14
		OLED    SCL -> PB6   SDA -> PB7（软件 I2C，与从机 DHT20 同引脚）

	本机角色：主机（接收端）
		1. 初始化 USART1 / LoRa(USART3) / LED / 蜂鸣器 / OLED
		2. 把 LoRa 模块配置成和从机完全一致的参数：
		   地址 0x0000、信道 0x3D(471MHz)、9600 8N1、空中速率 2.4k、透明传输
		3. 进入主循环：
		   3.1 收到从机发来的一帧 -> 校验 -> 解析 -> OLED 刷新显示
		   3.2 同时把原始帧转发到 PC 串口助手，方便对照调试
		   3.3 PC 串口助手输入的内容原样发给 LoRa（可手动发指令）

	OLED 显示格式（4 行）
		温度:26.3
		湿度:51.1
		水位:60
		无雨 / 小雨 / 中雨 / 大雨

	【注意】本机不再采集任何传感器，只做接收与显示。
================================================================================
*/

#define USART1_BRR (9600)
#define LORA_BRR   (9600)

/* 主循环基础节拍（ms） */
#define LOOP_TICK_MS (10)

/*
 * 超过这么久没收到新数据，就显示"等待从机"（ms）
 * 【实时上报】从机现在每 1 秒上报一次，
 * 这里留 5 秒余量——连续 5 秒没数据基本就是断了。
 */
#define RX_TIMEOUT_MS (5000)

/*
 * 【重点】OLED 屏幕刷新周期（ms）
 * 数据每秒都会收到（实时更新到 master_data），
 * 但屏幕不跟着每帧刷，只有到了 10 秒才刷一次。
 * 这样既保证数据实时，又避免 OLED 频繁刷新导致闪烁。
 */
#define SCREEN_REFRESH_MS (OLED_REFRESH_MS)

/*
【为什么放静态区而不是栈上】
	启动文件 startup_stm32f10x_hd.s 里 Stack_Size 只有 0x400 = 1024 字节。
	主循环栈帧里若再放 256 字节接收缓冲区，叠加起来余量会很紧张，
	有栈溢出踩坏变量的风险，所以放到 .bss 静态区。
*/
static u8 s_rxBuf[USART3_BUFFER_SIZE];

/*
 * 【LoRa 分段拼帧】
 * AS32-TTL-100-C 通过空口收到一包后，会按 32 字节一块往串口吐，
 * 块与块之间会触发 IDLE 中断。所以主循环每次
 * TakeFrame 拿到的往往只是一节碎片，不是完整一帧。
 * 必须先用拼帧器攒成完整一行（以 CRLF 结尾）再交给解析器。
 */
static FrameAssembler_t s_frameAsm;
static u8 s_frameBuf[FRAME_ASM_BUF_SIZE];

/* 串口打印拼接缓冲：[RX] + 帧数据 + CRLF，一次性发出 */
/*
 * 【修复】USART1_TX_BUFFER_SIZE 只有 256 字节，
 * 而 USART1_SendBuffer() 对超长数据是直接截断丢弃的，
 * 所以 s_txLine 必须保证最终发送长度 <= 256。
 * 帧本身正常只有 70 字节左右，240 继续完全够用。
 */
static u8 s_txLine[240];
#define TX_LINE_MAX       (240)
#define USART1_TX_LIMIT   (256)


int main(void)
{
	LORA_Config config;
	char        version[32];
	u16         rxLen = 0;
	u32         idleMs = 0;
	u32         screenMs = 0;      /* OLED 屏幕刷新计时 */
	u8          showingWait = 0;
	u8          screenDirty = 1;   /* 1 = 下次刷新时需要刷屏 */
	u8          beepOn = 0;        /* 蜂鸣器当前状态 */
	u8          prevAlarm = 0;     /* 上一次报警状态，用于检测跳变 */
	u32         beepMs = 0;        /* 报警时蜂鸣器闪烁计时 */

	/* ---------------- 1. 硬件初始化 ---------------- */
	USART1_Init(USART1_BRR); /* USART1 -> PC 串口调试助手 */
	LORA_Init(LORA_BRR);     /* USART3 -> LoRa 模块 */
	Beep_Init();
	Led_Init();

	USART1_SendString("======== LoRa MASTER (RX + OLED) ========\r\n");

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
		【重点】收发双方模块的地址、信道、空中速率必须保持一致！
		从机用的是：地址 0x0000 / 信道 0x3D / 9600 8N1 / 2.4k / 透明传输
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
		USART1_SendString("LORA CONFIG SET FAIL (module may be offline, still trying to receive)\r\n");
	}

	/* ---------------- 5. 清掉配置阶段残留的应答数据 ---------------- */
	memset(&usart3_value, 0, sizeof(USART3_Data));
	memset(&usart1_value, 0, sizeof(USART1_Data));

	/* ---------------- 6. OLED 初始化，先显示等待画面 ---------------- */
	/* 初始化 LoRa 拼帧器（清空缓存） */
	FrameAsm_Reset(&s_frameAsm);

	Master_Init();

	USART1_SendString("Waiting for slave data via LoRa...\r\n\r\n");

	/* ---------------- 7. 主循环 ---------------- */
	while (1)
	{
		/*
		7.1 LoRa -> 主机：收帧、校验、解析、OLED 显示
			【修复竞态】用 USART3_TakeFrame()：关中断 -> 整体拷出 -> 清空 -> 开中断，
			不会因为主循环里 OLED 刷新耗时而丢掉中断刚收到的整帧。
		*/
		if (USART3_TakeFrame(s_rxBuf, &rxLen))
		{
			if (rxLen > 0)
			{
				u16 frameLen = 0;

				/*
				 * 【根因修复】LoRa 模块会把一帧拆成 32 字节一块吐出，
				 * 所以这里拿到的 rxLen 很可能只是一节碎片，
				 * 不能直接送去解析。
				 * 先喂给拼帧器，攒成完整一行（以 CRLF 结尾）再解析。
				 * 一次喂入可能拼出多帧（比如两帧连在一起到达），
				 * 所以用 while 循环取到没有为止。
				 */
				while (FrameAsm_Push(&s_frameAsm, s_rxBuf, rxLen,
				                     s_frameBuf, sizeof(s_frameBuf), &frameLen))
				{
					/*
					 * 【修复】拼出一帧完整数据，转发到 PC。
					 * 不能连续调 3 次 USART1_SendString/SendBuffer：
					 * USART1 只有一个 DMA 发送缓冲区，每次调用都会先
					 * USART1_WaitTxComplete() 等上一帧真正发完（ 9600bps
					 * 下 71 字节约 74ms），3 次调用相当于白等两次。
					 * 这里先拼成一个完整字符串再一次发出。
					 */
					{
						u16 k = 0;
						const char *tag = "[RX] ";

						while (tag[k] != 0)
						{
							s_txLine[k] = (u8)tag[k];
							k++;
						}
						/*
						 * 拼接上限取两者最小值：
						 *   TX_LINE_MAX    —— 本地缓冲容量
						 *   USART1_TX_LIMIT —— DMA 发送缓冲容量（超过会被截断）
						 * 正常帧只有 ~70 字节，两个上限都不会触发。
						 */
						if (((u32)k + frameLen + 2u <= TX_LINE_MAX) &&
						    ((u32)k + frameLen + 2u <= USART1_TX_LIMIT))
						{
							u16 m;
							for (m = 0; m < frameLen; m++)
							{
								s_txLine[k + m] = s_frameBuf[m];
							}
							k = (u16)(k + frameLen);
							s_txLine[k++] = '\r';
							s_txLine[k++] = '\n';
							USART1_SendBuffer(s_txLine, k);
						}
					}

					/*
					 * 【关键】解析成功只更新缓存，不立即刷屏！
					 * 数据实时到位（master_data 随时是最新值），
					 * 但屏幕刷新由下面的 10 秒定时器统一驱动。
					 */
					if (Master_ParseFrame(s_frameBuf, frameLen))
					{
						showingWait = 0;
						screenDirty = 1;
						Led0_Toggle; /* 每收到一帧有效数据，LED 翻转一次 */
					}
					else
					{
						USART1_SendString("[WARN] frame parse/checksum failed, ignored\r\n");
					}

					idleMs = 0;

					/* 后续碎片已全部在拼帧缓存里，
					 * 这里传入 len=0 继续扫描剩余数据 */
					rxLen = 0;
				}
			}
		}

		/*
		7.2 PC -> LoRa
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
		7.3 超时处理：长时间没收到数据就回到"等待从机"画面
			避免屏幕上一直显示过期数据，让人误以为是最新的
		*/
		idleMs += LOOP_TICK_MS;
		if (idleMs >= RX_TIMEOUT_MS)
		{
			if (!showingWait)
			{
				Master_ShowWaiting();
				showingWait = 1;

				/*
				 * 【修复】断线后必须清除报警状态与蜂鸣器，
				 * 否则如果断线前正在报警，waterAlarm 会保持 1，
				 * 蜂鸣器就会无限制一直响下去（既扰人又费电）。
				 * 同时把 prevAlarm 同步清 0，保证重连后能重新检测跳变。
				 */
				master_data.waterAlarm = 0;
				prevAlarm = 0;
				beepOn = 0;
				beepMs = 0;
				Beep_Ctrl(0);
			}
			idleMs = RX_TIMEOUT_MS; /* 防止溢出 */
		}

		/*
		7.4 OLED 屏幕刷新：每 SCREEN_REFRESH_MS（10 秒）刷一次
			数据每秒都在更新（实时），但屏幕不跟着刷，
			只有到了周期点才把当前最新数据渲染上屏。
			当水位超过阈值时，屏幕会立即刷新并显示报警
			（报警不等 10 秒，安全优先）。
		*/
		screenMs += LOOP_TICK_MS;

		/*
		 * 【报警优先】报警状态发生跳变时，
		 * 立即刷屏 + 立即启动蜂鸣器，完全不遵循 10 秒刷屏周期。
		 *
		 *   上升沿（水位刚超阈）：屏幕立刻显示“水位过高”，
		 *                       蜂鸣器立刻開始闪烁鸣叫
		 *   下降沿（水位回落）：屏幕立刻恢复正常显示，
		 *                       蜂鸣器立刻关闭
		 *
		 * 注意：报警状态由 Master_ParseFrame() 每帧重算，
		 * 不依赖刷屏周期，所以能做到“帧到即报”。
		 */
		if (master_data.waterAlarm != prevAlarm)
		{
			prevAlarm = master_data.waterAlarm;

			if (master_data.waterAlarm)
			{
				/* 报警启动：蜂鸣器立即响（不等 500ms 计时） */
				beepOn  = 1;
				beepMs  = 0;
				Beep_Ctrl(1);
			}
			else
			{
				/* 报警解除：蜂鸣器立即关 */
				beepOn = 0;
				beepMs = 0;
				Beep_Ctrl(0);
			}

			/* 立即刷屏（不等 10 秒周期） */
			if (!showingWait)
			{
				screenMs = 0;
				screenDirty = 0;
				Master_Display();
			}
		}

		if ((screenMs >= SCREEN_REFRESH_MS) && screenDirty && (!showingWait))
		{
			screenMs = 0;
			screenDirty = 0;
			Master_Display();
		}
		if (screenMs > SCREEN_REFRESH_MS * 2)
		{
			screenMs = SCREEN_REFRESH_MS; /* 防止溢出，但保留触发条件 */
		}

		/*
		7.5 水位过高报警：蜂鸣器闪烁鸣叫
			水位 > WATER_ALARM_THRESHOLD（70%）时，
			蜂鸣器每 500ms 翻转一次，形成“嘀—嘀—”的间隔报警声；
			水位回落后立即关闭。
		*/
		/*
		 * 报警期间蜂鸣器闪烁：每 500ms 翻转一次，
		 * 形成“嘀—嘀—”的间隔报警声。
		 * 注意：报警启动/解除的瞬间已在上面处理过，
		 * 这里只负责报警持续期间的闪烁。
		 */
		if (master_data.waterAlarm)
		{
			beepMs += LOOP_TICK_MS;
			if (beepMs >= 500)
			{
				beepMs = 0;
				beepOn = (u8)(!beepOn);
				Beep_Ctrl(beepOn);
			}
		}

		/* 7.6 轮询节拍 */
		delay_ms(LOOP_TICK_MS);
	}
}
