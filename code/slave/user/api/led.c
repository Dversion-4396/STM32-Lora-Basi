#include "led.h"

/**
 * @brief Led 模块初始化，需要配置两个 IO 引脚，对应
 *        PB5 引脚控制 DS0/LED0
 *        PE5 引脚控制 DS1/LED1
 *		  两个引脚工作模式都为【通用推挽输出模式，速率 2MHz】
 *        【默认电平为高电平模式】			
 */
void Led_Init(void) 
{
	// 1. RCC 时钟使能 GPIOB 和 GPIOE
	RCC->APB2ENR |= RCC_APB2_GPIOB_CLOCK_ENABLE
					| RCC_APB2_GPIOE_CLOCK_ENABLE;
	
	/*
	2. 配置 PB5 和 PE5 两个引脚都是【通用推挽输出模式，速率 2MHz】
	*/
	GPIOB->CRL &= ~(0x0F << 20);
	GPIOB->CRL |= (General_Push_And_Pull_Speed_2MHz << 20);
	
	GPIOE->CRL &= ~(0x0F << 20);
	GPIOE->CRL |= (General_Push_And_Pull_Speed_2MHz << 20);
	
	// 3. 配置 PB5 和 PE5 两个引脚默认电平为高电平
	GPIOB->ODR |= (0x01 << 5);
	GPIOE->ODR |= (0x01 << 5);
}

/**
 * @brief 控制 Led0 亮灭
 *
 * @param flag 参数为 1 Led0 点亮，参数为 0 Led0 熄灭
 */
void Led0_Ctrl(u8 flag) 
{
	if (flag)
	{
		GPIOB->ODR &= ~(0x01 << 5);
	}
	else
	{
		GPIOB->ODR |= (0x01 << 5);
	}

}

/**
 * @brief 控制 Led1 亮灭
 *
 * @param flag 参数为 1 Led1 点亮，参数为 0 Led1 熄灭
 */
void Led1_Ctrl(u8 flag) 
{
	if (flag)
	{
		GPIOE->ODR &= ~(0x01 << 5);
	}
	else
	{
		GPIOE->ODR |= (0x01 << 5);
	}
}
