#ifndef _RS485_H
#define _RS485_H

#include "common.h"
#include "usart2.h"

/*
定义当前 485 芯片对应控制输入模式和输出模式引脚定义
*/
#define RS485_RE_DE_PIN       GPIO_Pin_7
#define RS485_RE_DE_PIN_GROUP GPIOD

/**
 * @brief RS485 模块初始化函数，要求提供的参数为对应 485 
 *      传输速率对应波特率。需要赋值给 USART 模块
 * 
 * @param brr 当前 485 模块传输速率
 */
void RS485_Init(u16 brr);

/**
 * @brief RS485 模块数据发送函数，需要通过控制 PD7 引脚电平来切换 485 
 *      芯片工作模式
 *      PD7 高电平 ==》 RE = 1 DE = 1 485 芯片通过 AB 端子对外发送数据
 *      PD7 低电平 ==》 RE = 0 DE = 0 485 芯片接收 AB 端子传递数据。
 * 
 * @param data  需要发送数据内存地址
 * @param count 需要发送的数据字节数
 */
void RS485_SendBuffer(u8 *data, u16 count);

#endif

