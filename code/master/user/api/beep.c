#include "beep.h"


/**
 * @brief Beep 蜂鸣器模块初始化，配置引脚为 PB8 引脚，
 *      工作模式为【通用推挽输出模式，速率 2MHz】
 */
void Beep_Init(void)
{
	// 1. 时钟使能 GPIOB 模块
	RCC->APB2ENR |= RCC_APB2_GPIOB_CLOCK_ENABLE;
	
	// 2. GPIOB PB8 引脚配置为【通用推挽输出模式，速率 2MHz】
	GPIOB->CRH &= ~(0x0F);
	GPIOB->CRH |= General_Push_And_Pull_Speed_2MHz;
	
	// 3. 【默认电平】为低电平，当前 Beep 停止工作
	GPIOB->ODR &= ~(0x01 << 8);
}


/**
 * @brief Beep 模块工作控制
 *
 * @param flag 提供参数 1 Beep 工作，提供参数 0 Beep 停止工作
 */
void Beep_Ctrl(u8 flag) 
{
	if (flag)
	{
		GPIOB->ODR |= (0x01 << 8);
	}
	else
	{
		GPIOB->ODR &= ~(0x01 << 8);
	}
}
