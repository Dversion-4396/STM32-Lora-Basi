#ifndef _SENSOR_REPORT_H
#define _SENSOR_REPORT_H

#include "common.h"
#include "raindrop.h"
#include "level.h"

/*
================================================================================
【模块说明】传感器数据采集 + LoRa 无线上报
--------------------------------------------------------------------------------
	功能：
		1. 周期采集雨滴传感器（DO 数字量 + AO 模拟量 -> 雨量等级）
		2. 周期采集 DHT20 温湿度（软件模拟 I2C，PB6=SCL / PB7=SDA）
		3. 把两路数据打包成一帧，通过 AS32-TTL-100-C LoRa 模块（USART3）发出去
		4. 同时把同一帧内容用 USART1 打印到 PC 串口助手，方便对照调试

	数据流：
		雨滴传感器 PC1(DO) / PC2(AO)  ┐
		                              ├─> 组帧 ─> LORA_SendData() ─> USART3(PB10/PB11) ─> LoRa 天线
		DHT20 PB6/PB7 (软件 I2C)      ┘        └─> USART1 打印（PA9/PA10）

	【引脚分配】
		雨滴 DO -> PC1   数字量，普通数字 IO 即可（上拉输入）
		雨滴 AO -> PC2   模拟量，必须接带 ADC 通道的引脚：PC2 = ADC1_IN12
		                STM32F103 的 ADC 通道固定映射，PE 口没有 ADC 通道，
		                所以 AO 不能接 PE0/PE1 等 PE 引脚。
		DHT20  SCL -> PB6 / SDA -> PB7   软件模拟 I2C，需 4.7K~10K 上拉

	【重要】DHT20 与板载 AT24C02 共用 PB6/PB7 总线
		AT24C02 地址 0x50，DHT20 地址 0x38，地址不冲突可共存；
		但两者都是开漏 I2C 器件，必须保证总线空闲后再发起传输。
		本模块只在主循环中顺序访问，不并发，因此安全。

	【重要】DHT20 单次测量耗时约 80~120ms，期间 CPU 被阻塞，
		所以上报周期不要设得太短（默认 10s 一次，见 REPORT_INTERVAL_MS）。
================================================================================
*/

/*
 * 数据上报周期（ms）
 * 默认 10000 = 10 秒上报一次。
 * DHT20 单次测量约 100~200ms，雨滴 AO 均值采样约 1ms，
 * 整个 Report_Once() 阻塞约 300~600ms，10 秒周期余量充足。
 * 若需调整上报频率，只改这一个宏即可。
 */
#define REPORT_INTERVAL_MS (10000)

/*
 * ============================ 帧格式说明 ============================
 * 采用定长 ASCII 文本帧，对端串口助手可直接阅读，无需解析二进制。
 *
 * 帧结构：  $RAIN,DHT20,T=...,H=...,ADC=...,V=...,WET=...,DO=...,LEVEL=...*XX\r\n
 *           ^                               ^                              ^  ^
 *           |                               |                              |  +-- 校验和(2字节HEX,大写)
 *           |                               |                              +----- 校验和起始符 "*"
 *           |                               +------------------------------------ 数据字段区
 *           +-------------------------------------------------------------------- 固定帧头(ASCII "$RAIN,DHT20,T=")
 *
 * 示例：
 *   $RAIN,DHT20,T=27.5,H=51.1,ADC=1850,V=1490,WET=54,DO=1,LEVEL=1*48\r\n
 *
 * 字段含义：
 *   T     温度，单位 ℃，保留一位小数；DHT20 读取失败时输出 "--"
 *   H     湿度，单位 %RH，保留一位小数；读取失败时输出 "--"
 *   ADC   雨滴 AO 原始 ADC 值，范围 0~4095；数值越小表示越湿
 *   V     雨滴 AO 换算电压，单位 mV（= ADC * 3300 / 4095）
 *   WET   湿润百分比，范围 0~100；0=全干 100=全湿
 *   DO    数字量电平：1=无水(高电平)  0=有水(低电平)
 *   LEVEL 雨量等级：0=无雨 1=小雨 2=中雨 3=大雨
 *   *XX   校验和：对 "*" 之前的所有字节做累加，取低 8 位，
 *         以两位大写十六进制 ASCII 表示
 *
 * 最长帧长度约 70 字节（含 CRLF 与结束符），远小于 LoRa 模块 256 字节 FIFO 上限。
 * ====================================================================
 */
#define REPORT_FRAME_MAX (96)

/* 一帧采集结果的解析结构，供其它模块引用 */
typedef struct
{
	float    temperature; /* DHT20 温度 ℃ */
	float    humidity;    /* DHT20 湿度 %RH */
	uint8_t  dht20Ok;     /* 1 = DHT20 本次读取成功 */
	uint8_t  dht20Err;    /* DHT20 错误码，见 dht20.h 的 DHT20_ERR_xxx */
	RainData_t rain;      /* 雨滴传感器完整数据 */
	LEVEL_Value level;    /* 水位传感器完整数据 */
} ReportData_t;

/**
 * @brief 上报模块初始化：初始化雨滴传感器、DHT20、以及 USART1 调试口
 * @note  必须在 USART1_Init() 与 LORA_Init() 之后调用
 */
void Report_Init(void);

/**
 * @brief 采集一次传感器数据（雨滴 + DHT20）
 * @param pData 输出结构体指针
 */
void Report_Collect(ReportData_t *pData);

/**
 * @brief 把采集结果按文本帧格式打包
 * @param pData  采集结果
 * @param pFrame 输出缓冲区，长度至少 REPORT_FRAME_MAX
 * @return 帧的实际字节数（不含结尾 '\0'）
 */
uint16_t Report_BuildFrame(const ReportData_t *pData, uint8_t *pFrame);

/**
 * @brief 采集 -> 组帧 -> LoRa 发送 -> USART1 打印，完整执行一次上报
 * @return 1 = LoRa 已发出  0 = 未发出
 */
uint8_t Report_Once(void);


#endif
