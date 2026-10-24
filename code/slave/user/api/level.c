#include "level.h"
#include "delay.h"

/**
 * @brief 液位模块全局数据，供上层串口打印、LCD 显示、报警判断使�? *        【重点】全局变量在头文件中用 extern 声明，在本文件中定义
 */
LEVEL_Value level_value = {0};

/*
【液位分段门限表�?	1. 数据来源：参考例�?main.c �?yewei �?if-else 判定链，原样搬过�?	2. 数组必须【从大到小】排列，因为判定时是从高液位往低液位逐段比对
	3. 【重点】门限值不用改的原因：XPT2046 �?STM32 ADC 都是 12 位，
	   参考电压都�?3.3V，满量程码值都�?4095，两者量纲完全一�?	4. 每往下一档，液位百分比减 10
*/
static const u16 level_threshold[LEVEL_SEGMENT_COUNT] =
{
	1100,	/* ADC >= 1100  ==>  100% */
	 900,	/* ADC >=  900  ==>   90% */
	 600,	/* ADC >=  600  ==>   80% */
	 440,	/* ADC >=  440  ==>   70% */
	 340,	/* ADC >=  340  ==>   60% */
	 300,	/* ADC >=  300  ==>   50% */
	 250,	/* ADC >=  250  ==>   40% */
	 220,	/* ADC >=  220  ==>   30% */
	 190,	/* ADC >=  190  ==>   20% */
	 160		/* ADC >=  160  ==>   10% */
};

/**
 * @brief 水位传感器模块初始化
 *
 *        【注意】本函数只做 GPIO 配置�?ADC 时钟分频�? *        ADC1 的整体初始化（模式、校准）�?RainDrop_AO_GPIO_Init() 完成�? *        因为两个模块共用同一�?ADC1，只能初始化一次�? */
void Level_Init(void)
{
	/* 1. 使能 GPIOA 时钟（ADC1 时钟由雨滴模块统一使能�?*/
	RCC->APB2ENR |= RCC_APB2_GPIOA_CLOCK_ENABLE
				  | RCC_APB2_ADC1_CLOCK_ENABLE;

	/*
	 * 2. PA1 配置为【模拟输入模式】，用于接入传感�?AO
	 *    CRL 寄存器每 4 位控制一个引脚，PA1 对应 bit7:4
	 *    MODE1[1:0] = 00 【输入模式�?	 *    CNF1 [1:0] = 00 【模拟输入】，此模式下引脚上的上下拉被断开
	 */
	GPIOA->CRL &= ~(0x0F << 4);
	GPIOA->CRL |=  (0x00 << 4);

	/*
	 * 3. ��˵����������ֻʹ��ˮλ�������� AO ģ���������� DO ��������
	 *    ԭ��DO �ڲο�������� PA4���� PA4 �� DAC_OUT1 / WIFI��λ /
	 *    ����EN����������ģ��ӿ��������׺� LoRa ģ����棬�ʲ�ʹ�á�
	 *    ˮλ�ж���ȫ�� AO �� ADC ֵ���㣬�����㹻��
	 */

	/*
	 * 5. ADC 时钟配置
	 *    【重点】参考手�?11.1 节：ADC 输入时钟 ADCCLK 不得超过 14MHz�?	 *    它由 PCLK2 经专用预分频器分频得�?	 *    PCLK2 = 72MHz�? 分频�?ADCCLK = 12MHz，符合要�?	 */
	RCC_ADCCLKConfig(RCC_PCLK2_Div6);
}

/**
 * @brief 读取一次传感器 AO 引脚对应�?ADC 转换结果
 *
 *        【重点】每次调用前都重新配置规则通道�? *        因为 ADC1 的通道 12（雨�?PC2）和通道 1（水�?PA1）共用同一个转换器�? *        上电后必须切换通道才能采到正确引脚�? *
 * @return 12 �?ADC 码值，范围 0 ~ 4095
 */
u16 Level_ReadAdc(void)
{
	u16 adc = 0;
	u32 timeout;

	/*
	 * 1. 设置规则组第 1 个转换的通道和采样时�?	 *    1.1 ADC_Channel_1 对应 PA1，也就是传感�?AO
	 *    1.2 Rank = 1 表示这是规则组里�?1 个转换的通道
	 *    1.3 采样时间 239.5 �?ADCCLK 周期（高阻抗信号源需要长采样�?	 */
	ADC1->SMPR1 &= ~(7UL << 3);          /* 通道1 �?SMPR2，这里不影响 */
	ADC1->SMPR2 &= ~(7UL << 3);          /* 清通道1的采样时间位 */
	ADC1->SMPR2 |=  (7UL << 3);          /* 通道1: 239.5 周期 */

	ADC1->SQR1 &= ~(0xFUL << 20);        /* 序列长度 = 1 个转�?*/
	ADC1->SQR3 &= ~(0x1FUL << 0);        /* �?SQ1 */
	ADC1->SQR3 |=  (1UL << 0);           /* SQ1 = 通道 1 (PA1) */

	/*
	 * 2. 使能 ADC 并启动转�?	 *    第一次写 ADON 唤醒，等 tSTAB 后再�?ADON + SWSTART 启动
	 */
	ADC1->CR2 |= (1UL << 0);
	delay_us(10);
	ADC1->CR2 = (1UL << 0) | (1UL << 20) | (7UL << 17) | (1UL << 22);

	/*
	 * 3. 等待转换结束，EOC 标志位置 1 表示 DR 寄存器里已经是本次结�?	 *    带超时保护，避免 ADC 异常时死�?	 */
	timeout = 200000;
	while (((ADC1->SR & (1UL << 1)) == 0) && (--timeout != 0))
	{
		;
	}

	if (timeout == 0)
	{
		return 0;   /* 超时返回 0，由上层有效性判断处�?*/
	}

	/*
	 * 4. 读取 DR 数据寄存�?	 *    【重点】读 DR 这个动作本身会自动清�?EOC 标志，不需要额外清�?	 */
	adc = (u16)(ADC1->DR & 0x0FFF);

	return adc;
}

/**
 * @brief 连续采样多次并求平均值，抑制随机干扰
 *
 * @return 平均值滤波后�?12 �?ADC 码�? */
u16 Level_ReadAdcFilter(void)
{
	u8  i;
	u32 sum = 0;

	/*
	 * 1. 累加 LEVEL_SAMPLE_TIMES 次采样结�?	 *    【重点】sum 必须定义�?u32�?0 �?4095 累加最大可�?40950�?	 *    �?u16 装不下，会溢出得到一个错误的小�?	 */
	for (i = 0; i < LEVEL_SAMPLE_TIMES; i++)
	{
		sum += Level_ReadAdc();
	}

	/* 2. 求平均值，把和除以采样次数 */
	return (u16)(sum / LEVEL_SAMPLE_TIMES);
}

/**
 * @brief ADC 码值换算成实际电压值，V = adc * 3300 / 4095
 *
 * @param adc 12 �?ADC 码�? * @return 换算后的电压值，单位 mV
 */
u16 Level_AdcToVoltage(u16 adc)
{
	/*
	 * 【重点】必须先强制转换�?u32 再相�?	 * 4095 * 3300 = 13513500，已经超�?u16 的最大�?65535�?	 * 如果直接写成 adc * LEVEL_VREF_MV / LEVEL_ADC_MAX�?	 * 编译器会�?u16 运算，中间结果溢出，算出来是错的
	 */
	return (u16)((u32)adc * LEVEL_VREF_MV / LEVEL_ADC_MAX);
}

/**
 * @brief ADC 码值换算成液位百分�? *
 * @param adc 12 �?ADC 码�? * @return 液位百分�?0 ~ 100
 */
u8 Level_AdcToPercent(u16 adc)
{
	u8 i;

	/*
	 * 1. �?100% 这一档开始逐段向下比对，第一个命中的档位就是当前液位
	 *    等价于参考例程里�?if - else if - else 判定�?	 */
	for (i = 0; i < LEVEL_SEGMENT_COUNT; i++)
	{
		if (adc >= level_threshold[i])
		{
			/* �?0 段对�?100%，第 1 段对�?90%，依次每段减 10 */
			return (u8)(100 - i * 10);
		}
	}

	/* 2. 所有门限都没命中，说明液位低于 10%，按 0% 处理 */
	return 0;
}

/**
 * @brief 液位百分比换算成液位高度
 *
 * @param percent 液位百分�?0 ~ 100
 * @return 液位高度，单�?mm，量�?0 ~ 90mm
 */
u8 Level_AdcToHeight(u8 percent)
{
	/*
	 * 1. 量程 90mm 按百分比线性折算，percent = 100 时结果为 90mm
	 *    【重点】先�?u16 再乘，percent * 90 最�?9000�?	 *    直接�?u8 运算同样会溢�?	 */
	return (u8)((u16)percent * LEVEL_RANGE_MM / 100);
}


/**
 * @brief 液位数据一次性刷新，结果写入全局结构�?level_value
 */
void Level_Update(void)
{
	u16 first;

	/* 1. 采样并滤波，得到本次 ADC 码�?*/
	level_value.adc = Level_ReadAdcFilter();

	/* 2. 由码值依次换算出电压、百分比、液位高�?*/
	/*    【重点】高度的入参是百分比，不�?ADC 码�?*/
	level_value.voltage = Level_AdcToVoltage(level_value.adc);
	level_value.percent = Level_AdcToPercent(level_value.adc);
	level_value.height  = Level_AdcToHeight(level_value.percent);

	/*
	 * [NEW] noise floor: height <= LEVEL_IGNORE_BELOW_MM means no water.
	 * Dry sensor noise lands in the 10% bucket -> 9mm fake reading.
	 * Force percent/height to 0 so the report shows 0% with no water.
	 */
	if (level_value.height <= LEVEL_IGNORE_BELOW_MM)
	{
		level_value.height  = 0;
		level_value.percent = 0;
	}

	/* 3. 读取 DO 数字量输出电�?*/

	/*
	 * 4. 有效性判断：连续多次采样完全不变且贴边（0 �?4095�?	 *    说明 PA1 悬空或传感器没接�?	 *    真实传感器即使静止也会有 1~5 �?LSB 抖动�?	 */
	{
		u8 i;
		u8 changed = 0;

		first = level_value.adc;
		for (i = 0; i < 5; i++)
		{
			delay_us(200);
			if (Level_ReadAdc() != first)
			{
				changed = 1;
				break;
			}
		}

		if ((!changed) && ((level_value.adc <= 10u) || (level_value.adc >= 4085u)))
		{
			level_value.valid = 0;
		}
		else
		{
			level_value.valid = 1;
		}
	}
}
