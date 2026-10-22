#include "tim3_ch2_pb5_pwm.h"

/**
 * @brief 配置 TIM3 通道2 CH2 部分重映射模式控制 PB5 引脚输出 PWM
 *			可以看着对应 LED 亮灭控制
 * 
 * @param psc 预分频器倍数，数据范围在 u16 范围以内
 * @param arr 自动重装载寄存器数据 ，数据范围在 u16 范围以内
 */
void TIM3_CH2_PB5_PWM(u16 psc, u16 arr)
{
	// 1. RCC 时钟使能，TIM3 定时器，GPIOB 和 AFIO
	RCC->APB2ENR |= RCC_APB2_GPIOB_CLOCK_ENABLE 
					| RCC_APB2_AFIO_CLOCK_ENABLE;
	RCC->APB1ENR |= RCC_APB1_TIM3_CLOCK_ENABLE;
	
	/*
	2. GPIOB 对应 PB5 引脚配置，【复用推挽输出模式，速率 2MHz】
	*/
	GPIOB->CRL &= ~(0x0F << 20);
	GPIOB->CRL |= (Alternate_Push_And_Pull_Speed_2MHz << 20);
	
	/*
	3. 关闭定时器，进行定时器配置
	*/
	TIM3->CR1 &= ~(0x01);
	
	/*
	4. AFIO 模块对当前 TIM3 输入输出通道对应 IO 引脚进行重映射操作
	选择部分重映射方式，对应寄存器 AFIO->MAPR 【位 11:10 TIM3_REMAP】
	*/
	AFIO->MAPR &= ~(0x03 << 10);
	AFIO->MAPR |= (0x02 << 10);
	
	/*
	5. TIM3 CH2 配置
		5.1 PSC + ARR 控制定时器工作周期，对应 PWM 输出周期
		5.2 CCMR 配置当前 CH2 通道对应 PWM 软件触发方式，快速输出模式 + 禁止预装载
		5.3 CCER 配置当前 CH2 通道对应【有效电平为低电平】，同时开启当前 CH2 输出
		5.4 启动定时器，CR1 DIR 默认是向上计数模式，可以明确修改
	*/
	// 5.1 PSC + ARR 控制定时器工作周期，对应 PWM 输出周期
	TIM3->PSC = psc - 1;
	TIM3->ARR = arr - 1;
	
	// 5.2 CCMR 配置当前 CH2 通道对应 PWM 软件触发方式，快速输出模式 + 禁止预装载
	TIM3->CCMR1 &= ~(0x03 << 8);  // CC2S 配置为 00，明确 CH2 为输出模式
	TIM3->CCMR1 |= (0x01 << 10);  // OC2FE 配置为 1，开始快速输出模式
	TIM3->CCMR1 &= ~(0x01 << 11); // OC2PE 配置为 0，关闭 ARR 预装载，随时可修改
	TIM3->CCMR1 &= ~(0x07 << 12); 
	TIM3->CCMR1 |= (0x06 << 12);  // OC2M 配置为 110, 当前 CH2 输出模式对应 PWM1 模式
	TIM3->CCMR1 &= ~(0x01 << 15); // OC2CE 关闭外部触发
	
	// 5.3 CCER 配置当前 CH2 通道对应【有效电平为低电平】，同时开启当前 CH2 输出
	TIM3->CCER |= (0x01 << 5);
	TIM3->CCER |= (0x01 << 4);
	
	// 5.4 启动定时器，CR1 DIR 默认是向上计数模式，可以明确修改
	TIM3->CR1 &= ~(0x01 << 3); // OPM 为 0，周期性定时器
	TIM3->CR1 &= ~(0x01 << 4); // DIR 为 0，CNT 向上计数
	TIM3->CR1 &= ~(0x01 << 7); // ARR 缓冲关闭
	TIM3->CR1 |= (0x01);       // 开始定时 TIM3 工作
}

void TIM3_CH2_PB5_PWM_Set_CCR2(u16 ccr2)
{
	TIM3->CCR2 = ccr2;
}

