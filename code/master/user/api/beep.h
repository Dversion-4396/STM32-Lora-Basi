#ifndef _BEEP_H
#define _BEEP_H

#include "common.h"

// 控制 Beep 工作状态反转
#define Beep_Toggle GPIOB->ODR ^= (0x01 << 8)

/**
 * @brief Beep 蜂鸣器模块初始化，配置引脚为 PB8 引脚，
 *      工作模式为【通用推挽输出模式，速率 2MHz】
 */
void Beep_Init(void);

/**
 * @brief Beep 模块工作控制
 *
 * @param flag 提供参数 1 Beep 工作，提供参数 0 Beep 停止工作
 */
void Beep_Ctrl(u8 flag);

#endif
