#ifndef _LED_H
#define _LED_H

#include "common.h"

// 控制 LED 亮灭反转宏定义
#define Led0_Toggle GPIOB->ODR ^= (0x01 << 5)
#define Led1_Toggle GPIOE->ODR ^= (0x01 << 5)

/**
 * @brief Led 模块初始化，需要配置两个 IO 引脚，对应
 *        PB5 引脚控制 DS0/LED0
 *        PE5 引脚控制 DS1/LED1
 *		  两个引脚工作模式都为【通用推挽输出模式，速率 2MHz】
 *        【默认电平为高电平模式】			
 */
void Led_Init(void);

/**
 * @brief 控制 Led0 亮灭
 *
 * @param flag 参数为 1 Led0 点亮，参数为 0 Led0 熄灭
 */
void Led0_Ctrl(u8 flag);

/**
 * @brief 控制 Led1 亮灭
 *
 * @param flag 参数为 1 Led1 点亮，参数为 0 Led1 熄灭
 */
void Led1_Ctrl(u8 flag);

#endif
