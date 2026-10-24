#ifndef _USART3_H
#define _USART3_H

#include "common.h"
// USART3 接收到的数据需要通过 USART1 打印�?PC �?
#include "usart1.h"

/*
【版本说明�?
	当前文件由寄存器版本改写为标准库（STM32F10x_StdPeriph_Driver V3.5.0）版本，
	对外提供的函数接口与寄存器版本完全一致，可以直接替换使用
*/

#define USART3_BUFFER_SIZE (256)

typedef struct usart3_data
{
    // u8 类型缓冲区数组，用于存放数据内容
    u8 data[USART3_BUFFER_SIZE];
    // data 数组中，有效字节个数
    u16 count;
    /*
  	情况一: 当前数据尚未接收完成，flag = 0
  	情况�? 当前数据接收完成，触�?IDLE 中断，flag = 1
  	情况�? data 数据接收缓冲区已满，flag = 1
    */
    u8 flag;
} USART3_Data;

extern USART3_Data usart3_value;

/**
 * @brief USART3 模块初始化函数，要求用户提供对应工作波特�?
 *			Tx ==> PB10 【复用推挽输出模式�?
 *	        Rx ==> PB11	【浮空输入模式�?
 *          NRZ 数据格式�?8N1 模式
 *
 * @param brr 用户提供的波特率参数，需要根据当�?MCU 时钟频率
 *				进行合理选择
 */
void USART3_Init(u32 brr);

/**
 * @brief USART3 模块发送一个字节数据到接收�?
 *
 * @param byte 需要进行发送的一个字节数�?
 */
void USART3_SendByte(u8 byte);

/**
 * @brief USART3 模块指定字节个数数据内容到接收端
 *
 * @param buffer 发送数据对应内存空间首地址
 * @param len    发送目标数据字节数
 */
void USART3_SendBuffer(u8 *buffer, u16 len);

/**
 * @brief USART3 模块发送字符串数据到接收端
 *
 * @param str 发送的字符串数据内�?
 */
void USART3_SendString(const char *str);


/**
 * @brief ��ȫȡ�� USART3 �յ���һ֡���ݣ����жϱ������������жϾ�̬��֡��
 *        ԭ��ͬ USART1_TakeFrame()����� usart1.h �е�˵����
 *
 * @param dst  ����������������� >= USART3_BUFFER_SIZE
 * @param pLen ������������ر�֡ʵ���ֽ���
 * @return 1 = ȡ��һ֡  0 = ��ǰû��������
 */
static __inline u8 USART3_TakeFrame(u8 *dst, u16 *pLen)
{
	u8 got = 0;

	__disable_irq();

	if (usart3_value.flag)
	{
		*pLen = usart3_value.count;
		memcpy(dst, usart3_value.data, usart3_value.count);
		memset(&usart3_value, 0, sizeof(USART3_Data));
		got = 1;
	}

	__enable_irq();

	return got;
}
#endif
