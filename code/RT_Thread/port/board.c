/*
 * Copyright (c) 2006-2019, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2021-05-24                  the first version
 */

#include <stdint.h>
#include <rthw.h>
#include <rtthread.h>

#include "stm32f10x.h"

#include "usart1.h"
#if defined(RT_USING_USER_MAIN) && defined(RT_USING_HEAP)
/*
 * Please modify RT_HEAP_SIZE if you enable RT_USING_HEAP
 * the RT_HEAP_SIZE max value = (sram size - ZI size), 1024 means 1024 bytes
 * 
 * 当前 RTT 启动对应的栈内存申请，对应的栈空间大小是 15 KB, 需要根据当前 RTT 使用组件，或者
 * 启动资源情况，以及当前 MCU RAM 大小来合理分配。
 */
#define RT_HEAP_SIZE (15 * 1024)
static rt_uint8_t rt_heap[RT_HEAP_SIZE];

RT_WEAK void *rt_heap_begin_get(void)
{
    return rt_heap;
}

RT_WEAK void *rt_heap_end_get(void)
{
    return rt_heap + RT_HEAP_SIZE;
}
#endif

void rt_os_tick_callback(void)
{
    rt_interrupt_enter();

    rt_tick_increase();

    rt_interrupt_leave();
}

/**
 * This function will initial your board.
 * 
 * 当前 MCU 启动的初始化函数，主要作用
 *      1. 当前 RTT 系统嘀嗒配置，根据当前 MCU 的时钟配置当前
 *          RTT 工作对应 Tick ，一般要求 Tick 时间对应 1 ms
 *      2. 可以将部分当前 MCU 启动资源配置加入到 rt_hw_board_init 函数
 */
void rt_hw_board_init()
{
    /* System Clock Update */
    SystemCoreClockUpdate();

    /* System Tick Configuration 配置当前 OS Tick 为 1 ms*/
    SysTick_Config(SystemCoreClock / RT_TICK_PER_SECOND);

    /* Call components board initial (use INIT_BOARD_EXPORT()) */
#ifdef RT_USING_COMPONENTS_INIT
    rt_components_board_init();
#endif

#if defined(RT_USING_USER_MAIN) && defined(RT_USING_HEAP)
    rt_system_heap_init(rt_heap_begin_get(), rt_heap_end_get());
#endif
}

void SysTick_Handler(void)
{
    /* enter interrupt */
    rt_interrupt_enter();

    rt_tick_increase();

    /* leave interrupt */
    rt_interrupt_leave();
}

#ifdef RT_USING_CONSOLE

static int uart_init(void)
{
    // #error "TODO 2: Enable the hardware uart and config baudrate."
    USART1_Init(115200);
    return 0;
}
INIT_BOARD_EXPORT(uart_init);

void rt_hw_console_output(const char *str)
{
    // #error "TODO 3: Output the string 'str' through the uart."
    USART1_SendString(str);
}

#endif
