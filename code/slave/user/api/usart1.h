#ifndef _USART1_H
#define _USART1_H

#include "common.h"

//中断缓冲区大�?
#define USART1_BUFFER_SIZE (256)
//DMA 发送缓冲区大小，单帧最长字节数
#define USART1_TX_BUFFER_SIZE (256)

typedef struct usart1_data
{
    // u8 类型缓冲区数组，用于存放数据内容
    u8 data[USART1_BUFFER_SIZE];
    // data 数组中，有效字节个数
    u16 count;
    /*
  	情况一: 当前数据尚未接收完成，flag = 0
  	情况�? 当前数据接收完成，触�?IDLE 中断，flag = 1
  	情况�? data 数据接收缓冲区已满，flag = 1
    */
    u8 flag;
} USART1_Data;

extern USART1_Data usart1_value;

/**
 * @brief USART1 模块初始化函数，需要用户提供对应的波特�?
 *			Tx ==> PA9  【复用推挽输出模式�?
 *	        Rx ==> PA10	【浮空输入模式�?
 *          NRZ 数据格式�?8N1 模式
 *
 * @param brr 用户提供的波特率参数，需要根据当�?MCU 时钟频率
 *				进行合理选择
 */
void USART1_Init(u32 brr);

/**
 * @brief USART1 模块发送一个字节数据到接收�?
 *
 * @param byte 需要进行发送的一个字节数�?
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
 * @param str 发送的字符串数据内�?
 */
void USART1_SendString(const char *str);

/**
 * @brief 等待当前 DMA 发送帧真正完成（数据全部移到串口线上）
 *			【重点】USART1_SendBuffer() 返回时数据可能还在发送，
 *				需要确保发完再切波特率/关串口时，调用本函数
 */
void USART1_WaitTxComplete(void);


/**
 * @brief ��ȫȡ�� USART1 �յ���һ֡���ݣ����жϱ������������жϾ�̬��֡��
 *
 *        ���ⱳ�����жϷ���������ֽ���ȫ�� usart1_value д�룬��ѭ���ж� flag
 *        �� memset ��ա�����ѭ�������ǰ������������ USART1_SendBuffer �ȴ�
 *        DMA ���Ҫ��ʮ���룩���ڼ��жϿ�����������һ֡����ѭ���� memset �Ͱ��������
 *        �������ڹ��жϵ��ٽ��������"���� + ���"����˲��ᶪ֡��
 *
 * @param dst  ����������������� >= USART1_BUFFER_SIZE
 * @param pLen ������������ر�֡ʵ���ֽ���
 * @return 1 = ȡ��һ֡  0 = ��ǰû��������
 */
static __inline u8 USART1_TakeFrame(u8 *dst, u16 *pLen)
{
	u8 got = 0;

	__disable_irq();

	if (usart1_value.flag)
	{
		*pLen = usart1_value.count;
		memcpy(dst, usart1_value.data, usart1_value.count);
		memset(&usart1_value, 0, sizeof(USART1_Data));
		got = 1;
	}

	__enable_irq();

	return got;
}
#endif
