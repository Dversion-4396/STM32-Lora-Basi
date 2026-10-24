#ifndef _COMMON_H
#define _COMMON_H

// 导入项目所需必要 STM32F10x 资源头文件
#include "stm32f10x.h"

#include "stdio.h"
#include "stdlib.h"
#include "string.h"

// 时钟使能宏定义
// APB2 分支对应模块使能位对应宏定义
// AFIO 模块对应时钟使能标志位
#define RCC_APB2_AFIO_CLOCK_ENABLE (0x01)

// 对应 GPIOA ~ GPIOG GPIO 模块对应时钟使能标志位
#define RCC_APB2_GPIOA_CLOCK_ENABLE (0x01 << 2)
#define RCC_APB2_GPIOB_CLOCK_ENABLE (0x01 << 3)
#define RCC_APB2_GPIOC_CLOCK_ENABLE (0x01 << 4)
#define RCC_APB2_GPIOD_CLOCK_ENABLE (0x01 << 5)
#define RCC_APB2_GPIOE_CLOCK_ENABLE (0x01 << 6)
#define RCC_APB2_GPIOF_CLOCK_ENABLE (0x01 << 7)
#define RCC_APB2_GPIOG_CLOCK_ENABLE (0x01 << 8)

// 对应 USART1 模块对应时钟使能标志位
#define RCC_APB2_USART1_CLOCK_ENABLE (0x01 << 14)

// APB1 分支对应模块使能位对应宏定义
// 定时器模块
#define RCC_APB1_TIM2_CLOCK_ENABLE (0x01)
#define RCC_APB1_TIM3_CLOCK_ENABLE (0x01 << 1)
#define RCC_APB1_TIM4_CLOCK_ENABLE (0x01 << 2)
#define RCC_APB1_TIM5_CLOCK_ENABLE (0x01 << 3)
#define RCC_APB1_TIM6_CLOCK_ENABLE (0x01 << 4)
#define RCC_APB1_TIM7_CLOCK_ENABLE (0x01 << 5)

// USART2 和 USART3 模块
#define RCC_APB1_USART2_CLOCK_ENABLE (0x01 << 17)
#define RCC_APB1_USART3_CLOCK_ENABLE (0x01 << 18)

/*
GPIO 通用推挽输出模式，不同速率对应宏定义
*/
#define General_Push_And_Pull_Speed_10MHz (0x01) // 0001
#define General_Push_And_Pull_Speed_2MHz (0x02)  // 0010
#define General_Push_And_Pull_Speed_50MHz (0x03) // 0011

/*
GPIO 通用开漏输出模式，不同速率对应宏定义
*/
#define General_Open_Drain_Output_Speed_10MHz (0x05) // 0101
#define General_Open_Drain_Output_Speed_2MHz (0x06)  // 0110
#define General_Open_Drain_Output_Speed_50MHz (0x07) // 0111

/*
GPIO 复用推挽输出模式，不同速率对应宏定义
*/
#define Alternate_Push_And_Pull_Speed_10MHz (0x09) // 1001
#define Alternate_Push_And_Pull_Speed_2MHz (0x0A)  // 1010
#define Alternate_Push_And_Pull_Speed_50MHz (0x0B) // 1011

/*
GPIO 上拉 or 下拉输入模式
*/
#define Pull_Up_Or_Pull_Down_Input_Mode (0x08) // 1000

/*
GPIO 浮空输入模式
*/
#define Floating_Input_Mode (0x04) // 0100
/*
================================================================================
【雨滴传感器模块引脚配置】
--------------------------------------------------------------------------------
    模块引脚：VCC -> 3.3V    GND -> GND
              DO  -> PC1   TTL 开关量，无水高电平、有水低电平
              AO  -> PC2   模拟量，接 ADC1_IN12

    【引脚选择依据】
        STM32F103 的 ADC 通道与引脚是固定映射，不能任意指定：
            ADC_IN0~IN7   -> PA0~PA7
            ADC_IN8~IN9   -> PB0~PB1
            ADC_IN10~IN15 -> PC0~PC5
            ADC3_IN4~IN8  -> PF6~PF10
        PE 口（PE0~PE15）没有任何 ADC 通道，所以 AO 不能接 PE0/PE1。\r\n        【注意】原用 PC0(ADC1_IN10) 实测 ADC 恒为 4095(悬空)，已改用 PC2。
        PC1/PC2 在本工程中未被其它模块占用（OLED/CAMERA 不使用），完全独立。
================================================================================
*/

// DO 数字量引脚配置（PC1，普通数字 IO 即可）
#define RAIN_DO_GPIO_PORT (GPIOC)
#define RAIN_DO_GPIO_PIN (GPIO_Pin_1)
#define RAIN_DO_GPIO_CLK (RCC_APB2Periph_GPIOC)

// AO 模拟量引脚 + ADC 配置（PC2 -> ADC1_IN12）
#define RAIN_AO_GPIO_PORT (GPIOC)
#define RAIN_AO_GPIO_PIN (GPIO_Pin_2)
#define RAIN_AO_GPIO_CLK (RCC_APB2Periph_GPIOC)
#define RAIN_AO_ADCx (ADC1)
#define RAIN_AO_ADC_CLK (RCC_APB2Periph_ADC1)
#define RAIN_AO_ADC_CHANNEL (ADC_Channel_12)
#define RAIN_ADC_SAMPLE_TIME (ADC_SampleTime_239Cycles5)

/*
 * ADC 转换超时计数（轮询次数上限）。
 * 主循环 72MHz 下一次空轮询约 20~30 周期，2 万次约 6~8ms，
 * 足够覆盖 239.5 周期采样时间（约 20us），又不会让程序卡太久。
 * 一旦超时，RainDrop_ReadAO_Raw() 返回上一次的采样值，保证程序不挂死。
 */
#define RAIN_ADC_TIMEOUT_CNT (20000)

// ADC 参考电压(mV) 与 12 位满量程值
#define RAIN_VREF_MV (3300)
#define RAIN_ADC_MAX_VALUE (4095)

// 采样滤波参数
#define RAIN_FILTER_TIMES (10)    // AO 均值滤波采样次数
#define RAIN_DO_DEBOUNCE_CNT (3)  // DO 消抖读取次数
#define RAIN_DO_DEBOUNCE_US (200) // DO 消抖每次读取间隔(us)

/*
================================================================================
【雨量等级阈值】直接按 AO 的 ADC 原始值判定（数值越小 = 水越多 = 雨越大）
--------------------------------------------------------------------------------
    ADC > 2500            ==> 无雨（干燥）
    ADC 2000 ~ 2500       ==> 小雨
    ADC 1500 ~ 2000       ==> 中雨
    ADC < 1500            ==> 大雨

    换算参考（VREF=3.3V，12 位）：
        ADC 2500 ≈ 2014 mV ≈ 湿润 39%
        ADC 2000 ≈ 1611 mV ≈ 湿润 51%
        ADC 1500 ≈ 1208 mV ≈ 湿润 63%

    实测验证：新板子干燥时 ADC 约 2900~3500，滴水后明显下降。
    如需调整灵敏度，只改下面这三个宏即可。
================================================================================
*/
#define RAIN_ADC_THRESHOLD_SMALL  (2500)  /* > 此值：无雨；<= 此值：开始有雨 */
#define RAIN_ADC_THRESHOLD_MIDDLE (2000)  /* <= 此值：中雨 */
#define RAIN_ADC_THRESHOLD_BIG    (1500)  /* <= 此值：大雨 */

/*
 * [FIX-BUG6] 这里原有两个无人引用的遗留宏（RAIN_REPORT_INTERVAL_MS、
 *            RAIN_USART_BAUDRATE），已删除，避免与 sensor_report.h 的
 *            REPORT_INTERVAL_MS 冲突。上报周期统一在 sensor_report.h 配置，
 *            串口波特率统一在 main.c 的 USART1_BRR / LORA_BRR 配置。
 */
#endif
