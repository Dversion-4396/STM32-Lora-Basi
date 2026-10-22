#ifndef _TIM6_H
#define _TIM6_H

#include "common.h"
#include "led.h"
#include "beep.h"

/**
 * @brief TIM6 基础定时器初始化函数，要求用户提供
 *          1. PSC 预分频器倍数
 *          2. ARR 自动重装载寄存器数据
 *
 * @param psc 预分频器倍数，数据范围在 u16 范围以内
 * @param arr 自动重装载寄存器数据 ，数据范围在 u16 范围以内
 */
void TIM6_Init(u16 psc, u16 arr);

#endif
