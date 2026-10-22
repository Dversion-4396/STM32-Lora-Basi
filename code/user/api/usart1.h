#ifndef _USART1_H
#define _USART1_H

#include "common.h"

#define USART1_BUFFER_SIZE (256)

typedef struct usart1_data
{
    // u8 类型缓冲区数组，用于存放数据内容
    u8 data[USART1_BUFFER_SIZE];
    // data 数组中，有效字节个数
    u16 count;
    /*
  	情况一: 当前数据尚未接收完成，flag = 0
  	情况二: 当前数据接收完成，触发 IDLE 中断，flag = 1
  	情况三: data 数据接收缓冲区已满，flag = 1
    */
    u8 flag;
} USART1_Data;

extern USART1_Data usart1_value;

/**
 * @brief USART1 模块初始化函数，要求用户提供对应工作波特率
 *			Tx ==> PA9  【复用推挽输出模式】
 *	        Rx ==> PA10	【浮空输入模式】
 *          NRZ 数据格式为 8N1 模式
 *
 * @param brr 用户提供的波特率参数，需要根据当前 MCU 时钟频率和
 *				进行合理选择
 */
void USART1_Init(u32 brr);

/**
 * @brief USART1 模块发送一个字节数据到接收端
 *
 * @param byte 需要进行发送的一个字节数据
 */
void USART1_SendByte(u8 byte);

/**
 * @brief USART1 模块指定字节个数数据内容到接收端
 *
 * @param buffer 发送数据对应内存空间首地址
 * @param len    发送目标数据字节数
 */
void USART1_SendBuffer(u8 *buffer, u16 len);

/**
 * @brief USART1 模块发送字符串数据到接收端
 *
 * @param str 发送的字符串数据内容
 */
void USART1_SendString(const char *str);


#endif
