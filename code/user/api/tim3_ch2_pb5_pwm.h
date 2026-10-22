#ifndef _TIM3_CH2_PB5_PWM_H
#define _TIM3_CH2_PB5_PWM_H

#include "common.h"

/**
 * @brief 配置 TIM3 通道2 CH2 部分重映射模式控制 PB5 引脚输出 PWM
 *			可以看着对应 LED 亮灭控制
 * 
 * @param psc 预分频器倍数，数据范围在 u16 范围以内
 * @param arr 自动重装载寄存器数据 ，数据范围在 u16 范围以内
 */
void TIM3_CH2_PB5_PWM(u16 psc, u16 arr);

void TIM3_CH2_PB5_PWM_Set_CCR2(u16 ccr2);

#endif

