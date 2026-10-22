#ifndef _KEY_INTERRUPT_H
#define _KEY_INTERRUPT_H

#include "common.h"
#include "led.h"
#include "beep.h"

/**
 * @brief 按键初始化函数，采用中断方式作为按键动作判断
 */
void Key_Interrupt_Init(void);

#endif
