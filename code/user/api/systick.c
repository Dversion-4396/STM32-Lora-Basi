#include "systick.h"

/**
 * @brief SysTick 初始化函数，通常情况下配置当前 SysTick 系统嘀嗒
 *      单位时间片为 1 ms
 */
void SysTick_Init(void)
{   
    /*
    SystemCoreClock Cortex-M3 内核程序中提供的用于描述当前
    MCU 时钟频率宏定义，并且当前宏定义可以根据不同底层芯片信号进行分别处理处理

    SystemCoreClock / 1000 使用 Cortex-M3 内核提供的初始化函数 SysTick_Config
    配置当前系统嘀嗒时间为 1 ms
    SysTick_Config 返回值 1 表示失败，0 表示成功，如果失败进入 while (1) 表示系统
    初始化失败。
    */
    if (SysTick_Config(SystemCoreClock / 1000))
    {
        while (1);
    }
}

/*
g_msTicks 全局变量 ms 单位 Tick 
*/
// uint32_t g_msTicks = 0;

/*
根据当前配置每 1 ms 触发一次 SysTick 定时器中断
当前 SysTick 对应的中断请求处理函数

【错误】 SysTick 中断处理函数不是  SysTick_IRQHandler 官方定义为
    SysTick_Handler，需要对 SysTick_Handler 函数执行实现。
    1. 当前文件实现 void SysTick_Handler(void) 函数
    2. 注释掉在 stm32f10x_it.c 中的 void SysTick_Handler(void) 函数内容
*/
// void SysTick_Handler(void)
// {
//     // g_msTicks 执行累加操作
//     g_msTicks += 1;
// }

// void Delay_ms(uint32_t ms)
// {
//     uint32_t startTick = g_msTicks;

//     // 阻塞延时方式，g_msTicks 数据累加 - startTick < ms 情况下未到达目标延时时间
//     // 当前 while 阻塞延时控制。
//     /*
//     【无符号整数回绕】可以按照 u8 类型数据分析
//     */
//     while ((g_msTicks - startTick) < ms);
// }

// void Delay_us(u32 us) 
// {
// 	while (us--) 
// 	{
// 		__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();
// 		__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();
// 		__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();
// 		__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();
// 		__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();
// 		__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();
// 		__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();
// 		__NOP();__NOP();
// 	}
// }