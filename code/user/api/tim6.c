#include "tim6.h"

/**
 * @brief TIM6 基础定时器初始化函数，要求用户提供
 *          1. PSC 预分频器倍数
 *          2. ARR 自动重装载寄存器数据
 *
 * @param psc 预分频器倍数，数据范围在 u16 范围以内
 * @param arr 自动重装载寄存器数据 ，数据范围在 u16 范围以内
 */
void TIM6_Init(u16 psc, u16 arr)
{
	// 1. RCC 时钟使能，对应 APB1 分支 TIM6 定时器使能
	RCC->APB1ENR |= RCC_APB1_TIM6_CLOCK_ENABLE;
	
	// 2. 定时器在配置时，必须保证处于关闭状态，要求 TIMx->CR1 对应 CEN 为 0
	TIM6->CR1 &= ~(0x01);
	
	/*
	3. 赋值当前 TIMx 对应的 PSC 和 ARR，
	要求赋值的数据为当前用户提供数据 - 1
	*/
	TIM6->PSC = psc - 1;
	TIM6->ARR = arr - 1;
	
	/*
	4. TIM6 CR1 中需要完成配置
		4.1 ARR 缓冲关闭
		4.2 当前定时器为周期性定时对应 OPM 为 0 
	*/
	TIM6->CR1 &= ~(0x01 << 7); // ARR 缓冲关闭
#if 1
	TIM6->CR1 &= ~(0x01 << 3); // OPM 为 0，周期性定时器
#else
	TIM6->CR1 |= (0x01 << 3); // OPM 为 1，一次性定时器
#endif

	/*
	5. 使能中断标志位 + 中断 IRQn 注册
	*/
	TIM6->DIER |= (0x01);
	
	NVIC_SetPriority(TIM6_IRQn, 2);
	NVIC_EnableIRQ(TIM6_IRQn);
	
	/*
	6. 启动定时器工作
	*/
	TIM6->CR1 |= (0x01);
}

/*
TIM6 基础定时器对应的中断处理函数
*/
void TIM6_IRQHandler(void)
{
	// 清除中断标志位
	TIM6->SR = 0;
	
	Led0_Toggle;
	Beep_Toggle;
}























