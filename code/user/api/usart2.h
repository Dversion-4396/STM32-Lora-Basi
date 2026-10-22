#ifndef _USART2_H
#define _USART2_H

#include "common.h"

#include "usart1.h"

#define USART2_BUFFER_SIZE (256)

typedef struct usart3_data
{
    // u8 类型缓冲区数组，用于存放数据内容
    u8 data[USART2_BUFFER_SIZE];
    // data 数组中，有效字节个数
    u16 count;
    /*
  	情况一: 当前数据尚未接收完成，flag = 0
  	情况二: 当前数据接收完成，触发 IDLE 中断，flag = 1
  	情况三: data 数据接收缓冲区已满，flag = 1
    */
    u8 flag;
} USART2_Data;

extern USART2_Data usart2_value;

/**
 * @brief USART2 模块初始化函数，对应 GPIO 引脚为
 *          PA2 ==> USART2_TX
 *          PA3 ==> USART2_RX
 * 
 * @param brr 用户提供的参数当前 USART2 模块波特率
 */
void USART2_Init(u32 brr);

void USART2_SendByte(u8 byte);
void USART2_SendBuffer(u8 *buffer, u16 count);
void USART2_SendString(const char *str);

#endif
