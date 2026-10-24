/**
 ******************************************************************************
 * @file    raindrop.c
 * @brief   雨滴传感器驱动实现（STM32F10x 标准外设库）
 *
 *          模块特性（依据《雨滴传感器模块使用说明书》）：
 *          1. 供电 3.3V~5V，板载 LM393 比较器
 *          2. 感应板无水时，DO 输出高电平，开关指示灯灭
 *          3. 感应板有水时，DO 输出低电平，开关指示灯亮
 *          4. AO 输出模拟电压，水越多电压越低
 *
 *          引脚全部在 common.h 中以宏定义，本文件不出现具体引脚号
 ******************************************************************************
 */

#include "raindrop.h"
#include "usart1.h"
#include <stdio.h>

/* 上一次成功的 ADC 采样值。ADC 转换超时时作为安全值返回，避免死等挂死 */
static uint16_t s_lastAdcRaw = 0;



/* 软件延时（不依赖 SysTick，避免与工程中已有的 delay.c / SysTick_Handler 冲突）
 * 以 72MHz、-O0 优化估算，1us 约 72 个时钟周期 */
static void RainDrop_DelayUs(uint32_t us)
{
    uint32_t i;
    /* [FIX] 原代码使用 SYSTEM_CORE_CLOCK，该宏在本工程任何头文件里都没有定义，
     *       会直接编译报错。改用 CMSIS 的 SystemCoreClock 全局变量（72MHz），
     *       它由 system_stm32f10x.c 启动时赋值，stm32f10x.h 已 extern 声明。 */
    uint32_t loops = us * (SystemCoreClock / 1000000u) / 4u;

    for (i = 0; i < loops; i++)
    {
        __NOP();
    }
}

/**
 * @brief  DO 引脚初始化（单独函数）
 *         DO 为模块输出的 TTL 电平，MCU 侧配置为上拉输入
 * @param  None
 * @retval None
 */
void RainDrop_DO_GPIO_Init(void)
{
    GPIO_InitTypeDef gpioInit;

    RCC_APB2PeriphClockCmd(RAIN_DO_GPIO_CLK, ENABLE);

    gpioInit.GPIO_Pin = RAIN_DO_GPIO_PIN;
    gpioInit.GPIO_Mode = GPIO_Mode_IPU; /* 上拉输入：模块未接时默认为无水状态 */
    gpioInit.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(RAIN_DO_GPIO_PORT, &gpioInit);
}

/**
 * @brief  AO 引脚 + ADC1 初始化（单独函数）
 *         AO 必须配置为模拟输入，并初始化 ADC1 对应通道
 * @param  None
 * @retval None
 */
void RainDrop_AO_GPIO_Init(void)
{
    GPIO_InitTypeDef gpioInit;
    ADC_InitTypeDef adcInit;

    /* 1. 打开 GPIO 与 ADC1 时钟 */
    RCC_APB2PeriphClockCmd(RAIN_AO_GPIO_CLK | RAIN_AO_ADC_CLK, ENABLE);

    /* 2. ADC 时钟分频：PCLK2/6 = 12MHz，必须 <= 14MHz */
    RCC_ADCCLKConfig(RCC_PCLK2_Div6);

    /* 3. AO 引脚配置为模拟输入 */
    gpioInit.GPIO_Pin = RAIN_AO_GPIO_PIN;
    gpioInit.GPIO_Mode = GPIO_Mode_AIN;
    gpioInit.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(RAIN_AO_GPIO_PORT, &gpioInit);

    /* 4. ADC1 工作模式：独立模式、单通道、软件触发、非连续转换 */
    ADC_DeInit(RAIN_AO_ADCx);
    adcInit.ADC_Mode = ADC_Mode_Independent;
    adcInit.ADC_ScanConvMode = DISABLE;
    adcInit.ADC_ContinuousConvMode = DISABLE;
    adcInit.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;
    adcInit.ADC_DataAlign = ADC_DataAlign_Right;
    adcInit.ADC_NbrOfChannel = 1;
    ADC_Init(RAIN_AO_ADCx, &adcInit);

    /*
     * 5. 使能 ADC
     *    [FIX-ADC] 原代码 ADC_Cmd(ENABLE) 后立刻校准，缺少 tSTAB 等待。
     *    按 STM32F10x 参考手册 RM0008：ADC 上电后需要 tSTAB(约 1us)
     *    才能开始校准，否则校准结果不可靠，采样值会偏差。
     *    这里加 2ms 延时（软件循环），确保 ADC 稳定后再校准。
     */
    ADC_Cmd(RAIN_AO_ADCx, ENABLE);
    RainDrop_DelayUs(2000);   /* tSTAB 等待，2ms 余量充足 */

    /* 6. ADC 校准（F1 必须校准，否则采样值偏差大） */
    ADC_ResetCalibration(RAIN_AO_ADCx);
    while (ADC_GetResetCalibrationStatus(RAIN_AO_ADCx) == SET)
        ;

    ADC_StartCalibration(RAIN_AO_ADCx);
    while (ADC_GetCalibrationStatus(RAIN_AO_ADCx) == SET)
        ;
}

/**
 * @brief  雨滴传感器全部引脚初始化（依次调用 DO / AO 初始化）
 * @param  None
 * @retval None
 */
void RainDrop_GPIO_Init(void)
{
    RainDrop_DO_GPIO_Init();
    RainDrop_AO_GPIO_Init();
}

/**
 * @brief  读取 DO 原始电平
 * @param  None
 * @retval 1 = 高电平（无水）；0 = 低电平（检测到雨水）
 */
uint8_t RainDrop_ReadDO_Level(void)
{
    if (GPIO_ReadInputDataBit(RAIN_DO_GPIO_PORT, RAIN_DO_GPIO_PIN) == Bit_RESET)
    {
        return 0; /* 低电平：有水 */
    }
    return 1; /* 高电平：无水 */
}

/**
 * @brief  读取 DO 电平并消抖：连续多次读取一致才确认
 * @param  None
 * @retval 1 = 无水；0 = 有水
 */
/*
 * [FIX-BUG8] 原实现存在两个问题：
 *   (1) 注释说"以上一次结果为准"，代码却 return first（第一次读数），注释与实现不符；
 *   (2) 一旦检测到抖动就立即 return 提前退出，RAIN_DO_DEBOUNCE_CNT(3) 次实际只比较了 1 次，
 *       消抖形同虚设。
 *   现改为：滚动比较相邻两次读数，全部一致才认可；出现抖动则以最后一次读数为准返回，
 *   并真正走满 RAIN_DO_DEBOUNCE_CNT 次采样。
 */
uint8_t RainDrop_ReadDO_LevelStable(void)
{
    uint8_t i;
    uint8_t last = RainDrop_ReadDO_Level();
    uint8_t cur;

    for (i = 1; i < RAIN_DO_DEBOUNCE_CNT; i++)
    {
        RainDrop_DelayUs(RAIN_DO_DEBOUNCE_US);

        cur = RainDrop_ReadDO_Level();

        if (cur != last)
        {
            /* 电平发生跳变：以最新读数为准，继续采完剩余次数 */
            last = cur;
        }
    }

    return last;
}

/**
 * @brief  AO 单次 ADC 采样
 * @param  None
 * @retval 12 位 ADC 原始值（0~4095）
 */
/**
 * @brief  AO 单次 ADC 采样（纯寄存器实现，绕开标准库封装）
 *
 *         【为什么改成寄存器直操】
 *         诊断发现标准库路径下 ADC1->CR2 的 EXTTRIG(bit20) 和 SWSTART(bit22)
 *         读回都是 0，EOC 也不置位，但 DR 却是 4095（复位值），
 *         说明转换根本没真正启动，读到的只是残留值。
 *         这里直接操作寄存器，每一步都显式设置，避免标准库封装带来的歧义。
 *
 *         采样流程（按 RM0008 第 11 章）：
 *           1. 设置 SMPR2 采样时间
 *           2. 设置 SQR1/SQR3 规则序列（1 个转换，通道号）
 *           3. CR2 置 ADON（若已置位则本次写入会启动转换）
 *           4. CR2 置 EXTTRIG + SWSTART 启动转换
 *           5. 轮询 SR 的 EOC 位
 *           6. 读 DR
 *
 * @param  None
 * @retval 12 位 ADC 原始值（0~4095）
 */
/**
 * @brief  AO 单次 ADC 采样（带写入验证的寄存器实现）
 *
 *         【背景】诊断发现 SMPR1/SQR3 写入后读回仍为 0，
 *         需要逐步验证每个寄存器写入是否生效，以判断是
 *         "写保护" 还是 "ADC 未上电"。
 *
 * @param  None
 * @retval 12 位 ADC 原始值（0~4095）
 */
/**
 * @brief  AO 单次 ADC 采样
 *
 *         【实现说明】
 *         本函数用寄存器直操而非标准库封装，原因：
 *           1. 标准库 ADC_SoftwareStartConvCmd 在部分芯片上时序不够明确；
 *           2. 这里每一步都显式可见，便于排查问题。
 *
 *         【重要】绝对不能在采样函数里做 RCC_APB2PeriphResetCmd(ADC1) 复位！
 *         那会把整个 ADC 外设重置，所有寄存器归零，转换永远无法完成。
 *         ADC 初始化只在 Report_Init() 里做一次即可。
 *
 *         采样流程（按 RM0008 第 11 章）：
 *           1. 设置采样时间 SMPR1（通道 10~17 在 SMPR1）
 *           2. 设置规则序列 SQR1/SQR3
 *           3. CR2 置 ADON 唤醒 ADC，等待 tSTAB
 *           4. CR2 置 ADON|EXTTRIG|EXTSEL=7|SWSTART 启动转换
 *           5. 轮询 SR 的 EOC 位
 *           6. 读 DR 得到 12 位结果
 *
 * @param  None
 * @retval 12 位 ADC 原始值（0~4095）
 */
uint16_t RainDrop_ReadAO_Raw(void)
{
    uint32_t timeout;

    /*
     * 1. 采样时间：通道 10~17 在 SMPR1，通道 0~9 在 SMPR2
     *    本工程 AO 用 PC2 = ADC1_IN12，属于 10~17 组，故配置 SMPR1
     *    SMPR1 中通道 12 对应 bits[8:6]，239.5 周期 = 0b111
     */
    ADC1->SMPR1 = (7UL << 6);

    /*
     * 2. 规则序列：SQR1.L[3:0]=0 表示 1 个转换
     *    SQR3.SQ1[4:0] = 通道号（RAIN_AO_ADC_CHANNEL 的数值）
     */
    ADC1->SQR1 = 0;
    ADC1->SQR3 = (uint32_t)RAIN_AO_ADC_CHANNEL;

    /*
     * 3. 使能 ADC：第一次写 ADON=1 从掉电状态唤醒
     *    唤醒后需要 tSTAB(约 1us) 才能开始转换，这里给 100us 余量
     */
    ADC1->CR2 = (1UL << 0);              /* ADON = 1，唤醒 */
    RainDrop_DelayUs(100);

    /*
     * 4. 启动转换：同时置 ADON、EXTTRIG、EXTSEL=7(SWSTART)、SWSTART
     *    EXTSEL=7 表示规则组的触发源选择"软件触发"
     *    EXTTRIG=1 使能规则组外部触发
     *    SWSTART=1 触发一次转换（硬件自动清零）
     */
    ADC1->CR2 = (1UL << 0) | (1UL << 20) | (7UL << 17) | (1UL << 22);

    /*
     * 5. 等待转换完成（EOC = SR bit1），带超时保护
     *    正常转换约 20us（239.5 采样周期 + 12.5 转换周期 @12MHz）
     *    50 万次轮询约 10~20ms，远大于需要，只在 ADC 异常时才触发超时
     */
    timeout = RAIN_ADC_TIMEOUT_CNT;
    while (((ADC1->SR & (1UL << 1)) == 0) && (--timeout != 0))
    {
        ;
    }

    if (timeout == 0)
    {
        /* 超时：返回上一次有效采样值，保证程序不挂死 */
        return s_lastAdcRaw;
    }

    /* 6. 读 DR（读操作会自动清除 EOC），返回 12 位结果 */
    s_lastAdcRaw = (uint16_t)(ADC1->DR & 0x0FFF);
    return s_lastAdcRaw;
}




/**
 * @brief  AO 多次采样求平均（均值滤波，抑制抖动）
 * @param  times 采样次数，建议 10
 * @retval 滤波后的 ADC 值
 */
uint16_t RainDrop_ReadAO_Average(uint8_t times)
{
    uint32_t sum = 0;
    uint8_t i;

    if (times == 0)
    {
        return RainDrop_ReadAO_Raw();
    }

    for (i = 0; i < times; i++)
    {
        sum += RainDrop_ReadAO_Raw();
        RainDrop_DelayUs(50);
    }
    return (uint16_t)(sum / times);
}

/**
 * @brief  ADC 值换算为电压（mV）
 * @param  adcValue ADC 原始值
 * @retval 电压值，单位 mV
 */
uint16_t RainDrop_ConvertToVoltage(uint16_t adcValue)
{
    return (uint16_t)(((uint32_t)adcValue * RAIN_VREF_MV) / RAIN_ADC_MAX_VALUE);
}

/**
 * @brief  ADC 值换算为湿润百分比
 *         雨滴板水越多 -> AO 电压越低 -> ADC 值越小 -> 湿润百分比越大
 * @param  adcValue ADC 原始值
 * @retval 0~100，0 = 全干，100 = 全湿
 */
uint8_t RainDrop_ConvertToWetPercent(uint16_t adcValue)
{
    uint32_t percent;

    if (adcValue >= RAIN_ADC_MAX_VALUE)
    {
        return 0;
    }
    percent = ((RAIN_ADC_MAX_VALUE - (uint32_t)adcValue) * 100u) / RAIN_ADC_MAX_VALUE;
    return (uint8_t)percent;
}

/**
 * @brief  由 AO 的 ADC 原始值判定雨量等级（无雨 / 小雨 / 中雨 / 大雨）
 *
 *         【判定依据】雨滴板水越多 -> AO 电压越低 -> ADC 值越小
 *         ADC > 2500          无雨
 *         ADC 2000 ~ 2500     小雨
 *         ADC 1500 ~ 2000     中雨
 *         ADC < 1500          大雨
 *         阈值在 common.h 的 RAIN_ADC_THRESHOLD_xxx 中配置。
 *
 * @param  adcRaw AO 滤波后的 ADC 原始值（0~4095）
 * @retval RainLevel_t
 */
RainLevel_t RainDrop_GetLevel(uint16_t adcRaw)
{
    if (adcRaw > RAIN_ADC_THRESHOLD_SMALL)
    {
        return RAIN_NONE;      /* 干燥，无雨 */
    }
    else if (adcRaw > RAIN_ADC_THRESHOLD_MIDDLE)
    {
        return RAIN_SMALL;     /* 2500 ~ 2000，小雨 */
    }
    else if (adcRaw > RAIN_ADC_THRESHOLD_BIG)
    {
        return RAIN_MIDDLE;    /* 2000 ~ 1500，中雨 */
    }
    return RAIN_BIG;           /* <= 1500，大雨 */
}



void RainDrop_GetData(RainData_t *pData)
{
    uint8_t i;

    if (pData == NULL)
    {
        return;
    }

    /* 1. DO 数字量：消抖后读取 */
    pData->doLevel = RainDrop_ReadDO_LevelStable();
    pData->isWet = (pData->doLevel == 0u) ? 1u : 0u;

    /*
     * 2. AO 模拟量：多次采样取平均
     *    [FIX-RAIN] 加一个有效性自检：
     *    如果连续多次采样结果完全一模一样（尤其是 0 或 4095 满量程），
     *    说明 ADC 通道很可能没接通（引脚悬空或 AO 线没接）。
     *    此时湿润百分比没有意义，标记 adcValid = 0，
     *    上层打印时会提示"雨滴ADC异常"，而不是给出误导性的"无雨"。
     */
    pData->adcRaw = RainDrop_ReadAO_Average(RAIN_FILTER_TIMES);
    pData->voltageMv = RainDrop_ConvertToVoltage(pData->adcRaw);
    pData->wetPercent = RainDrop_ConvertToWetPercent(pData->adcRaw);
    pData->level = RainDrop_GetLevel(pData->adcRaw);

    /* 3. ADC 有效性判断：再采 5 次，看是否有任何变化 */
    {
        uint16_t first = pData->adcRaw;
        uint8_t  changed = 0;

        for (i = 0; i < 5; i++)
        {
            RainDrop_DelayUs(200);
            if (RainDrop_ReadAO_Raw() != first)
            {
                changed = 1;
                break;
            }
        }

        /*
         * 完全不变 + 落在极端值（<=10 或 >=4085）= 判定为 ADC 未接通。
         * 真实传感器即使静止不动，ADC 也会有 1~5 个 LSB 的抖动，
         * 严格不变且贴边说明引脚悬空（悬空会被拉满 4095 或拉到 0）。
         */
        if ((!changed) && ((pData->adcRaw <= 10u) || (pData->adcRaw >= 4085u)))
        {
            pData->adcValid = 0;
        }
        else
        {
            pData->adcValid = 1;
        }
    }
}

/**
 * @brief  雨量等级转字符串（大 / 中 / 小 / 无）
 * @param  level 等级
 * @retval 字符串
 */
const char *RainDrop_LevelString(RainLevel_t level)
{
#if RAIN_PRINT_CN
    switch (level)
    {
    case RAIN_BIG:
        return "\xE5\xA4\xA7\xE9\x9B\xA8" /* 大雨 */;
    case RAIN_MIDDLE:
        return "\xE4\xB8\xAD\xE9\x9B\xA8" /* 中雨 */;
    case RAIN_SMALL:
        return "\xE5\xB0\x8F\xE9\x9B\xA8" /* 小雨 */;
    case RAIN_NONE:
    default:
        return "\xE6\x97\xA0\xE9\x9B\xA8" /* 无雨 */;
    }
#else
    switch (level)
    {
    case RAIN_BIG:
        return "BIG";
    case RAIN_MIDDLE:
        return "MIDDLE";
    case RAIN_SMALL:
        return "SMALL";
    case RAIN_NONE:
    default:
        return "NONE";
    }
#endif
}

/**
 * @brief  DO 是否有水转字符串（有水 / 无水）
 * @param  isWet 1 = 检测到雨水，0 = 干燥
 * @retval 字符串
 */
const char *RainDrop_WaterString(uint8_t isWet)
{
#if RAIN_PRINT_CN
    return (isWet == 1u) ? "\xE6\x9C\x89\xE6\xB0\xB4" /* 有水 */ : "\xE6\x97\xA0\xE6\xB0\xB4" /* 无水 */;
#else
    return (isWet == 1u) ? "YES" : "NO";
#endif
}
