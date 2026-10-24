#ifndef _LEVEL_H
#define _LEVEL_H

#include "common.h"

/*
================================================================================
【模块说明】XPT2046/水位传感器信号转换板 驱动
--------------------------------------------------------------------------------
    硬件连接�?        VCC  ==>  5V      （模块要�?5V 供电�?        GND  ==>  GND
        AO   ==>  PA1     ��ģ������ADC1_IN1��
        DO   ==>  ����    ��������ֻʹ�� AO������ DO��

    信号特征�?        1. AO 模拟量：0 ~ 1.0V 对应液位 0% ~ 100%，与液位为【非线性】关�?        2. DO 数字量：板上 LM393 比较器输出，比较门限由可调电位器设定
        3. 传感器供�?5V，但 AO 最高只�?1.0V，远低于 STM32 ADC �?           输入上限 VREF+�?.3V），所以【可以直接接入，不需要电阻分压�?
    【重要】AO 用的 PA1 在开发板上兼�?TPAD 触摸检测脚�?        必须拔掉 P10 跳线帽，否则 PA1 会被 TPAD 电路干扰，读数不准�?
    【重要】本模块与雨滴传感器共用 ADC1（分时切换通道）：
        雨滴 AO = PC2 = ADC1_IN12
        水位 AO = PA1 = ADC1_IN1
        两个模块不能同时转换，但主循环里顺序采集，不会冲突�?        每次采集前都要重新配置规则通道，这是必须的�?================================================================================
*/

/*
【补充宏】common.h 中未定义�?ADC1 模块时钟使能标志�?    APB2ENR 寄存�?bit9 ==> ADC1EN
*/
#ifndef RCC_APB2_ADC1_CLOCK_ENABLE
#define RCC_APB2_ADC1_CLOCK_ENABLE (0x01 << 9)
#endif

/* 水位传感�?AO 接入�?ADC 通道：PA1 对应 ADC1 的通道 1 */
#define LEVEL_ADC_CHANNEL (ADC_Channel_1)

/* 水位百分比分段个数，与下�?level_threshold 数组长度保持一�?*/
#define LEVEL_SEGMENT_COUNT (10)

/* 单次数据刷新时的累加采样次数，用于平均值滤�?*/
#define LEVEL_SAMPLE_TIMES (10)

/* ADC 参考电压，单位 mV，开发板 VREF+ �?3.3V */
#define LEVEL_VREF_MV (3300)

/* 12 �?ADC 满量程码值，2^12 - 1 = 4095 */
#define LEVEL_ADC_MAX (4095)

/* 传感器量�?90mm，对应液�?100% */
#define LEVEL_RANGE_MM (90)

/*
 * [NEW] water level noise floor (mm)
 * Any height <= this value is treated as NO WATER (0%).
 * Reason: dry sensor ADC noise falls into the 10% bucket,
 *         10% * 90mm = 9mm, which showed a fake level with no water.
 * Set to 9 so the whole 10% bucket (ADC 160~189) is ignored.
 */
#define LEVEL_IGNORE_BELOW_MM (9)


/*
【液位模块数据结构�?    1. adc      : ADC 原始码值，范围 0 ~ 4095
    2. voltage  : 换算后的 AO 引脚电压值，单位 mV
    3. percent  : 液位百分比，范围 0 ~ 100，单�?%
    4. height   : 换算后的液位高度，单�?mm
    5. do_state : DO 数字量输出电平，1 为高电平�? 为低电平
    6. valid    : 数据有效性，1 = 正常�? = ADC 异常（引脚悬空等�?*/
typedef struct level_value
{
	u16 adc;		/* ADC 原始码�?0 ~ 4095 */
	u16 voltage;	/* AO 引脚电压，单�?mV */
	u8  percent;	/* 液位百分比，单位 % */
	u8  height;		/* 液位高度，单�?mm */
	u8  do_state;	/* DO 数字量输出电�?*/
	u8  valid;		/* 1 = 数据有效 */
} LEVEL_Value;

extern LEVEL_Value level_value;

/**
 * @brief 水位传感器模块初始化
 *        1. 使能 GPIOA �?ADC1 模块时钟
 *        2. PA1 配置为【模拟输入模式】，接传感器 AO
 *        3. �������̲�ʹ�� DO��ֻ�� AO ģ��������ռ�� PA4 ������ģ��ӿ�����
 *        4. ADC 时钟配置�?PCLK2 / 6 = 12MHz，不超过 14MHz 上限
 *
 * @note  ADC1 的整体初始化和校准由 RainDrop_AO_GPIO_Init() 负责�? *        本函数只配置 GPIO �?ADC 时钟分频，不重复初始�?ADC�? */
void Level_Init(void);

/**
 * @brief 读取一次传感器 AO 引脚对应�?ADC 转换结果
 *        【重要】每次调用前都重新配置规则通道，因�?ADC1 被雨滴共用�? *
 * @return 12 �?ADC 码值，范围 0 ~ 4095
 */
u16 Level_ReadAdc(void);

/**
 * @brief 连续采样多次并求平均值，抑制随机干扰
 *
 * @return 平均值滤波后�?12 �?ADC 码�? */
u16 Level_ReadAdcFilter(void);

/**
 * @brief ADC 码值换算成实际电压值，V = adc * 3300 / 4095
 *
 * @param adc 12 �?ADC 码�? * @return 换算后的电压值，单位 mV
 */
u16 Level_AdcToVoltage(u16 adc);

/**
 * @brief ADC 码值换算成液位百分�? *        【重点】传感器输出与液位为非线性关系，按门限表分段判定�? *              不能直接�?adc * 100 / 1241 做线性换�? *
 * @param adc 12 �?ADC 码�? * @return 液位百分�?0 ~ 100
 */
u8 Level_AdcToPercent(u16 adc);

/**
 * @brief 液位百分比换算成液位高度
 *
 * @param percent 液位百分�?0 ~ 100
 * @return 液位高度，单�?mm，量�?0 ~ 90mm
 */
u8 Level_AdcToHeight(u8 percent);


/**
 * @brief 液位数据一次性刷新，结果写入全局结构�?level_value
 *        1. 平均滤波采样一�?AO，得�?ADC 码�? *        2. 换算电压、百分比、液位高�? *        3. 顺带读取一�?DO 数字�? */
void Level_Update(void);

#endif
