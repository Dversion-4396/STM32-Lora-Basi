#include "sensor_report.h"
#include "raindrop.h"
#include "dht20.h"
#include "usart1.h"
#include "usart3.h"
#include "lora_as32.h"
#include "delay.h"

/*
================================================================================
【实现说明】传感器采集 + LoRa 上报
--------------------------------------------------------------------------------
	注意点 1：DHT20 与工程原有 delay.c 的延时冲突
		dht20.c 自带 DHT20_Delay_us / DHT20_Delay_ms（基于 DWT 周期计数器），
		与本工程 delay.c 的纯软件循环延时互不干扰，两者可以共存，不需要改。

	注意点 2：DHT20 与 AT24C02 共用 PB6/PB7
		本模块只在主循环顺序访问，不与其它 I2C 操作并发，因此安全。
		DHT20 驱动内部有 Bus_Recover()，即使总线被拉死也能自行恢复。

	注意点 3：LoRa 模块内部 FIFO 只有 256 字节，单帧不能超过 256 字节
		本帧最长约 80 字节，远小于上限，安全。

	注意点 4：USART1 打印用 DMA 方式（见 usart1.c），单次调用即可，
		不要逐字节调用 USART1_SendByte()，否则每个字节都要等一次 DMA 空闲。
================================================================================
*/

/* 校验和：累加所有字节后取低 8 位 */
static uint8_t Report_Checksum(const uint8_t *p, uint16_t len)
{
	uint8_t sum = 0;
	uint16_t i;

	for (i = 0; i < len; i++)
	{
		sum += p[i];
	}

	return sum;
}

/* [FIX-BUG7] 字节 -> 两字节十六进制 ASCII（大写 A~F，与 LORA 命令应答风格一致） */
static void Report_Hex2(uint8_t value, uint8_t *out)
{
	const char hex[] = "0123456789ABCDEF";

	out[0] = hex[(value >> 4) & 0x0F];
	out[1] = hex[value & 0x0F];
}

/* 把一个 uint32 十进制数字写入缓冲区，返回写入字节数 */
static uint16_t Report_UInt(uint8_t *out, uint32_t value)
{
	uint8_t tmp[10];
	uint8_t n = 0;
	uint16_t i;

	if (value == 0)
	{
		out[0] = '0';
		return 1;
	}

	while (value > 0)
	{
		tmp[n++] = (uint8_t)('0' + (value % 10));
		value /= 10;
	}

	for (i = 0; i < n; i++)
	{
		out[i] = tmp[n - 1 - i];
	}

	return n;
}

/* 前置声明：Report_PrintCN 用到，定义在后面 */
static uint16_t Report_Fixed1(uint8_t *out, float value);

/* 追加 C 字符串到输出缓冲，返回写入字节数（不含结尾 0） */
static uint16_t Report_Append(uint8_t *out, const char *s)
{
	uint16_t n = 0;

	while (s[n] != 0)
	{
		out[n] = (uint8_t)s[n];
		n++;
	}

	return n;
}

/*
 * 打印一行中文关键数据：
 *   温度:27.5℃,湿度:51.1%,无雨 [雨滴:4095]
 *   温度:27.5℃,湿度:51.1%,有雨(小雨) [雨滴:800]
 *   DHT20 读取失败时温湿度显示 --，错误原因单独打印一行。
 * 注意：本文件保存为 GBK 编码，串口助手请把编码设为 GBK 才能正常显示中文。
 */
static void Report_PrintCN(const ReportData_t *pData)
{
	uint8_t line[96];
	uint16_t n = 0;
	const char *rain;

	/* 温度 */
	n += Report_Append(&line[n], "温度:");
	if (pData->dht20Ok)
	{
		n += Report_Fixed1(&line[n], pData->temperature);
	}
	else
	{
		line[n++] = '-';
		line[n++] = '-';
	}

	/* 湿度 */
	n += Report_Append(&line[n], "℃,湿度:");
	if (pData->dht20Ok)
	{
		n += Report_Fixed1(&line[n], pData->humidity);
	}
	else
	{
		line[n++] = '-';
		line[n++] = '-';
	}
	n += Report_Append(&line[n], "%,");

	/*
	 * 雨情：完全由 AO 的 ADC 值判定（等级已在 RainDrop_GetData 里算好）
	 *   ADC > 2500       无雨
	 *   ADC 2000~2500    小雨
	 *   ADC 1500~2000    中雨
	 *   ADC <= 1500      大雨
	 */
	switch (pData->rain.level)
	{
	case RAIN_BIG:
		rain = "大雨";
		break;
	case RAIN_MIDDLE:
		rain = "中雨";
		break;
	case RAIN_SMALL:
		rain = "小雨";
		break;
	case RAIN_NONE:
	default:
		rain = "无雨";
		break;
	}
	n += Report_Append(&line[n], rain);

	/* 水位高度（mm），量程 0~90mm */
	n += Report_Append(&line[n], ",水位:");
	if (pData->level.valid)
	{
		n += Report_UInt(&line[n], pData->level.percent);
		n += Report_Append(&line[n], "%(ADC=");
		n += Report_UInt(&line[n], pData->level.adc);
		n += Report_Append(&line[n], ")");
	}
	else
	{
		n += Report_Append(&line[n], "--");
	}

	n += Report_Append(&line[n], "\r\n");
	USART1_SendBuffer(line, n);
}



/* 打印错误信息（只在出错时调用） */
static void Report_PrintErr(const ReportData_t *pData)
{
	const char *why;

	if (pData->dht20Ok)
	{
		return; /* 正常不打印任何东西 */
	}

	switch (pData->dht20Err)
	{
	case DHT20_ERR_TRIG:
		why = "DHT20 触发命令无应答(检查 SCL=PB6 SDA=PB7 接线与上拉)";
		break;
	case DHT20_ERR_STATUS:
		why = "DHT20 测量超时(传感器可能损坏)";
		break;
	case DHT20_ERR_READ:
		why = "DHT20 读数据无应答(总线被占用或接触不良)";
		break;
	case DHT20_ERR_CRC:
		why = "DHT20 CRC 校验失败(信号干扰或线太长)";
		break;
	default:
		why = "DHT20 未知错误";
		break;
	}

	USART1_SendString("[错误] ");
	USART1_SendString(why);
	USART1_SendString("\r\n");
}


/* 定点小数写入：把 value 乘以 10 后按 "整数部分.小数部分" 输出
 * 负数（温度可能为负）也能正确输出 */
static uint16_t Report_Fixed1(uint8_t *out, float value)
{
	int32_t v10;
	int32_t intPart;
	int32_t fracPart;
	uint16_t n = 0;

	v10 = (int32_t)(value * 10.0f + ((value >= 0.0f) ? 0.5f : -0.5f));

	if (v10 < 0)
	{
		out[n++] = '-';
		v10 = -v10;
	}

	intPart  = v10 / 10;
	fracPart = v10 % 10;

	n += Report_UInt(&out[n], (uint32_t)intPart);
	out[n++] = '.';
	out[n++] = (uint8_t)('0' + fracPart);

	return n;
}

/**
 * @brief 上报模块初始化
 */
void Report_Init(void)
{
	/* 1. 雨滴传感器：DO(PC1) + AO(PC2/ADC1_IN12) 引脚与 ADC 初始化 */
	RainDrop_GPIO_Init();

	/* 2. 水位传感器：只接 AO(PA1/ADC1_IN1)，不接 DO，不占用 PA4 */
	/*    【注意】ADC1 的整体初始化已由 RainDrop_AO_GPIO_Init 完成，
	 *    这里只配置 GPIO，不重复初始化 ADC。 */
	Level_Init();

	/* 3. DHT20 温湿度传感器：PB6/PB7 软件 I2C 初始化 */
	DHT20_Init();

	/* 启动信息只保留必要项：接线提示 + 上报周期 */
	USART1_SendString("\r\n雨滴传感器 + DHT20 温湿度 LoRa 上报\r\n");
	USART1_SendString("雨滴: DO=PC1  AO=PC2  |  DHT20: SCL=PB6  SDA=PB7\r\n");
	USART1_SendString("每 10 秒上报一次\r\n\r\n");
}

/**
 * @brief 采集一次传感器数据
 */
void Report_Collect(ReportData_t *pData)
{
	uint8_t attempt;

	if (pData == NULL)
	{
		return;
	}

	/* 1. 雨滴传感器：DO 消抖 + AO 均值滤波 + 电压/湿润度/等级换算 */
	RainDrop_GetData(&pData->rain);

	/* 2. 水位传感器：平均滤波 + 电压/百分比/高度换算
	 *    【注意】两个模块共用 ADC1，这里顺序采集，
	 *    每次采集都会重新配置规则通道，不会冲突。 */
	Level_Update();
	pData->level = level_value;

	/* 2. DHT20 温湿度：开漏总线偶发无应答，失败后隔 20ms 重试一次 */
	pData->dht20Ok  = 0;
	pData->dht20Err = DHT20_ERR_TRIG;

	for (attempt = 0; attempt < 2; attempt++)
	{
		pData->dht20Err = DHT20_Read(&pData->humidity, &pData->temperature);

		if (pData->dht20Err == DHT20_OK)
		{
			pData->dht20Ok = 1;
			break;
		}

		DHT20_Delay_ms(20);
	}

	/* 读取失败时给一个安全值，避免上报结构体里是野值 */
	if (!pData->dht20Ok)
	{
		pData->temperature = 0.0f;
		pData->humidity    = 0.0f;
	}
}

/**
 * @brief 按文本帧格式打包
 *        帧格式：$RAIN,DHT20,T=xx.x,H=xx.x,ADC=xxxx,V=xxxx,WET=xx,DO=x,LEVEL=x*XX\r\n
 *        其中 *XX 是 "*" 之前所有字节的校验和（两字节十六进制 ASCII，大写）
 */
uint16_t Report_BuildFrame(const ReportData_t *pData, uint8_t *pFrame)
{
	uint16_t n = 0;
	uint16_t bodyLen;
	uint8_t  sum;
	const char *prefix = "$RAIN,DHT20,T=";

	if ((pData == NULL) || (pFrame == NULL))
	{
		return 0;
	}

	/* 1. 固定前缀 */
	while (*prefix != '\0')
	{
		pFrame[n++] = (uint8_t)(*prefix++);
	}

	/* 2. 温度 T=xx.x */
	if (pData->dht20Ok)
	{
		n += Report_Fixed1(&pFrame[n], pData->temperature);
	}
	else
	{
		pFrame[n++] = '-';
		pFrame[n++] = '-';
	}

	/* 3. 湿度 H=xx.x */
	pFrame[n++] = ',';
	pFrame[n++] = 'H';
	pFrame[n++] = '=';
	if (pData->dht20Ok)
	{
		n += Report_Fixed1(&pFrame[n], pData->humidity);
	}
	else
	{
		pFrame[n++] = '-';
		pFrame[n++] = '-';
	}

	/* 4. 雨滴 AO 原始值 ADC=xxxx */
	pFrame[n++] = ',';
	pFrame[n++] = 'A';
	pFrame[n++] = 'D';
	pFrame[n++] = 'C';
	pFrame[n++] = '=';
	n += Report_UInt(&pFrame[n], pData->rain.adcRaw);

	/* 5. 雨滴 AO 电压 V=xxxx (mV) */
	pFrame[n++] = ',';
	pFrame[n++] = 'V';
	pFrame[n++] = '=';
	n += Report_UInt(&pFrame[n], pData->rain.voltageMv);

	/* 6. 湿润百分比 WET=xx */
	pFrame[n++] = ',';
	pFrame[n++] = 'W';
	pFrame[n++] = 'E';
	pFrame[n++] = 'T';
	pFrame[n++] = '=';
	n += Report_UInt(&pFrame[n], pData->rain.wetPercent);

	/* 7. DO 数字量 DO=x */
	pFrame[n++] = ',';
	pFrame[n++] = 'D';
	pFrame[n++] = 'O';
	pFrame[n++] = '=';
	pFrame[n++] = (uint8_t)('0' + (pData->rain.doLevel ? 1 : 0));

	/* 8. 雨量等级 LEVEL=x */
	pFrame[n++] = ',';
	pFrame[n++] = 'L';
	pFrame[n++] = 'E';
	pFrame[n++] = 'V';
	pFrame[n++] = 'E';
	pFrame[n++] = 'L';
	pFrame[n++] = '=';
	pFrame[n++] = (uint8_t)('0' + (uint8_t)pData->rain.level);

	/* 9. 水位百分比 WL=xx (0~100) */
	pFrame[n++] = ',';
	pFrame[n++] = 'W';
	pFrame[n++] = 'L';
	pFrame[n++] = '=';
	n += Report_UInt(&pFrame[n], pData->level.percent);

	/* 9. 校验和：对已写入的全部字节求和，取低 8 位，写成 *XX */
	bodyLen = n;
	sum = Report_Checksum(pFrame, bodyLen);

	pFrame[n++] = '*';
	Report_Hex2(sum, &pFrame[n]);
	n += 2;

	/* 10. 帧尾 CR LF */
	pFrame[n++] = '\r';
	pFrame[n++] = '\n';

	/* 11. 补字符串结束符，便于当字符串打印 */
	pFrame[n] = '\0';

	return n;
}

/**
 * @brief 采集 -> 组帧 -> LoRa 发送 -> USART1 打印，完整执行一次上报
 */
uint8_t Report_Once(void)
{
	ReportData_t data;
	uint8_t  frame[REPORT_FRAME_MAX];
	uint16_t len;

	/* 1. 采集两路传感器 */
	Report_Collect(&data);

	/* 2. 组帧 */
	len = Report_BuildFrame(&data, frame);
	if (len == 0)
	{
		return 0;
	}

	/* 3. 通过 LoRa 模块（USART3）把整帧原样发出去
	 *    LORA_SendData() 内部会先等 AUX 高电平（模块空闲）再发，
	 *    不会把数据塞进忙状态的模块里 */
	LORA_SendData(frame, len);

	/*
	 * 4. 本地串口只打印关键数据（中文）与错误信息，不再打印原始帧
	 *    格式：温度:27.5℃,湿度:51.1%,无雨 [雨滴:4095]
	 *    出错时额外打印一行 [错误] 原因
	 */
	Report_PrintCN(&data);
	Report_PrintErr(&data);

	return 1;
}
