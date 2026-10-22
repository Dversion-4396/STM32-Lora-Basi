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
#define General_Push_And_Pull_Speed_2MHz  (0x02) // 0010
#define General_Push_And_Pull_Speed_50MHz (0x03) // 0011

/*
GPIO 通用开漏输出模式，不同速率对应宏定义
*/
#define General_Open_Drain_Output_Speed_10MHz (0x05) // 0101
#define General_Open_Drain_Output_Speed_2MHz  (0x06) // 0110
#define General_Open_Drain_Output_Speed_50MHz (0x07) // 0111

/*
GPIO 复用推挽输出模式，不同速率对应宏定义
*/
#define Alternate_Push_And_Pull_Speed_10MHz (0x09) // 1001
#define Alternate_Push_And_Pull_Speed_2MHz  (0x0A) // 1010
#define Alternate_Push_And_Pull_Speed_50MHz (0x0B) // 1011

/*
GPIO 上拉 or 下拉输入模式
*/
#define Pull_Up_Or_Pull_Down_Input_Mode (0x08) // 1000


/*
GPIO 浮空输入模式
*/
#define Floating_Input_Mode (0x04) // 0100

#endif
