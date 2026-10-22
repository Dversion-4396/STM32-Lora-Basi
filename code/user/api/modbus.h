#ifndef _MODBUS_H
#define _MODBUS_H

#include "common.h"
#include "rs485.h"
#include "usart2.h"

// RS485 模块对应波特率数据
#define RS485_BRR (9600)
// 当前 RS485 单一字节传递时间
#define BYTE_INTERVAL_TIME (1000000 / (RS485_BRR / 10))

// 宏开关/宏标记，使用哪一个时间间隔，对应宏标志打开
#define USING_T3_5
// #define USING_T4_0

// T3.5 间隔周期时间
#ifdef USING_T3_5
/*
DATA_FRAME_TIME_INTERVAL 宏定义名称保持一致, 但是在不同的时间间隔宏开关情况下对应
数据不同，方便后续项目开发，并且不需要修改代码内容。
*/
#define DATA_FRAME_TIME_INTERVAL (BYTE_INTERVAL_TIME * 3 + (BYTE_INTERVAL_TIME >> 1))
#endif

// T4.0 间隔周期时间
#ifdef USING_T4_0
#define DATA_FRAME_TIME_INTERVAL (BYTE_INTERVAL_TIME * 4)
#endif

/**
 * @brief Modbus 数据接收控制定时，使用 TIM7 基础定时实现。
 */
void Modbus_TIM_Config(void);

/**
 * @brief 发送 Modbus 03 功能码对设备进行多个寄存器数据提取操作
 *
 * @param device_addr Modbus 从机设备设备地址
 * @param reg_addr    Modbus 从机设备对应读取寄存器起始地址
 * @param reg_count   期望读取的寄存器数量
 */
void Modbus_Send_03_Cmd(u8 device_addr, u16 reg_addr, u16 reg_count);

/*
数据解析函数是针对于不同的功能码信息处理
*/
u8 Modbus_Analysis_Temp_And_Humi(u8 *buffer, u16 count);
/**
 * @brief Modbus CRC-16 计算函数
 * 
 * @param data   待计算的数据指针
 * @param length 数据长度（字节数）
 * @return CRC校验值（16位）
 */
uint16_t Modbus_CRC16(const uint8_t *data, size_t length);



#endif
