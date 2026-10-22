#include "key_interrupt.h"

/**
 * @brief 按键初始化函数，采用中断方式作为按键动作判断
 */
void Key_Interrupt_Init(void)
{
	// 1. RCC 时钟使能 GPIOE 和 AFIO 模块
	RCC->APB2ENR |= RCC_APB2_AFIO_CLOCK_ENABLE 
					| RCC_APB2_GPIOE_CLOCK_ENABLE;
	
	// 2. PE4 GPIO 工作模式配置，要求【上拉输入模式】
	GPIOE->CRL &= ~(0x0F << 16);
	GPIOE->CRL |= (Pull_Up_Or_Pull_Down_Input_Mode << 16);
	GPIOE->ODR |= (0x01 << 4);
	
	// 3. EXTI4 外部中断控制线相关配置
	// 3.1 AFIO EXTICR2 配置当前 EXTI4 对应 IO 引脚为 PE4 引脚
	AFIO->EXTICR[1] &= ~(0x0F); 
	AFIO->EXTICR[1] |= (0x04); 
	
	// 3.2 配置 EXTI4 对应下降沿触发中断
	EXTI->FTSR |= (0x01 << 4);
	
	// 3.2 EXTI4 使能 中断
	EXTI->IMR |= (0x01 << 4);
	
	// 4. 中断注册
	NVIC_SetPriority(EXTI4_IRQn, 1);
	NVIC_EnableIRQ(EXTI4_IRQn);

}

// 实现 EXTI4_IRQn 对应的 EXTI4_IRQHandler 中断处理函数
void EXTI4_IRQHandler(void)
{
	// 判断当前触发中断是否为 EXTI4 中断
	if (EXTI->PR & (0x01 << 4))
	{
		// 清除中断操作，需要 W1
		EXTI->PR |= (0x01 << 4);
		
		// 控制灯光亮灭
		Led0_Toggle;
		Led1_Toggle;
		Beep_Toggle;
	}
}
