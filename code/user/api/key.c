#include "key.h"


/**
 * @brief 按键模块初始化函数，需要配置的内容有
 *		KEY_UP ==> PA0 对应工作模式以为【下拉输入模式】
 *	    KEY2 ==> PE2 对应工作模式以为【上拉输入模式】
 *	    KEY1 ==> PE3 对应工作模式以为【上拉输入模式】 
 *	    KEY0 ==> PE4 对应工作模式以为【上拉输入模式】
 */
void Key_Init(void) 
{
	// 1. RCC 时钟使能，需要使能模块 GPIOA 和 GPIOE
	RCC->APB2ENR |= RCC_APB2_GPIOA_CLOCK_ENABLE 
					| RCC_APB2_GPIOE_CLOCK_ENABLE;
	
	// 2. GPIO 工作模式配置
	/*
	KEY_UP ==> PA0 对应工作模式以为【下拉输入模式】
	*/
	GPIOA->CRL &= ~(0x0F);
	// 配置当前引脚工作模式为【上拉 or 下拉输入模式】
	GPIOA->CRL |= Pull_Up_Or_Pull_Down_Input_Mode;
	// PA0 ODR 设置为 0，明确当前引脚工作模式为【下拉模式】
	GPIOA->ODR &= ~(0x01);
	
	/*
	TODO 
		KEY2 ==> PE2 对应工作模式以为【上拉输入模式】
		KEY1 ==> PE3 对应工作模式以为【上拉输入模式】 
		KEY0 ==> PE4 对应工作模式以为【上拉输入模式】
	*/
}

/**
 * @brief 用于反馈当前按键模块哪一个按键被按下。
 *
 * @return 返回类型对应 Key_Value 声明的枚举内容
 */
Key_Value Key_Press(void)
{
	// 【判断 KEY_UP】 是否按下
	/*
	如果 KEY_UP 按下，对应 PA0 IDR 输入数据寄存器【位 0】为 1
	*/
	if (0x01 & GPIOA->IDR)
	{
		// 软件消抖，延时 20 ms
		delay_ms(20);
			
		if (0x01 & GPIOA->IDR) 
		{
			// 需要等待用户按键松开，完成整个按键动作，才可以返回当前
			// 按键已按下的明确信号。
			while (0x01 & GPIOA->IDR);
			
			// 明确当前 KEY_UP 被按下，返回 KEY_UP_PRESSED 数据
			return KEY_UP_PRESSED;
		}
	}
	
	// TODO 【判断 KEY2】 是否按下
	
	// TODO 【判断 KEY1】 是否按下
	
	// TODO 【判断 KEY0】 是否按下

	return NO_KEY_PRESSED;
}







