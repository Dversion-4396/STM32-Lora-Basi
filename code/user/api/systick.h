#ifndef _SYSTICK_H
#define _SYSTICK_H

#include "common.h"

/**
 * @brief SysTick 初始化函数，通常情况下配置当前 SysTick 系统嘀嗒
 *      单位时间片为 1 ms
 */
void SysTick_Init(void);

/**
 * @brief 基于 SysTick 毫秒延时函数
 * 
 * @param ms 用户提供的毫秒值。
 */
void Delay_ms(uint32_t ms);

/**
 * @brief 延时控制函数，控制时间单位为 us 微秒
 *
 * @param us 用户提供的控制延时的微秒数
 */
void Delay_us(u32 us);

#endif
