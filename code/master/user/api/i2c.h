#ifndef _I2C_H
#define _I2C_H
#include "common.h"
#include "delay.h"
#define I2C_SCL_PIN_HIGH GPIOB->ODR |= (0X01 << 6);
#define I2C_SCL_PIN_LOW GPIOB->ODR &= ~(0X01 << 6);
#define I2C_SDA_PIN_HIGH GPIOB->ODR |= (0X01 << 7);
#define I2C_SDA_PIN_LOW GPIOB->ODR &= ~(0X01 << 7);
#define I2C_DELAY_TIME (1)

/*
 * [NEW] Common I2C macros required by oled.c
 *   OLED_ADDR       : SSD1306 7-bit slave address (module spec 0x78 is 8-bit form)
 *   WRITE_FLAG      : R/W bit, 0 = write
 *   READ_FLAG       : R/W bit, 1 = read
 *   I2C_RETRY_COUNT : bus retry count on NACK
 */
#define OLED_ADDR        (0x3C)
#define WRITE_FLAG       (0x00)
#define READ_FLAG        (0x01)
#define I2C_RETRY_COUNT  (3)
typedef enum
{
    I2C_ACK,
    I2C_NACK
} I2C_ACK_t;
/// @brief i2c 模块初始化函数
///        PB6 和 PB7 配置为开漏输出，速度 2MHz
void I2C_GPIO_Init(void);
/// @brief 控制时钟线高低电平变化
/// @param flag 1 为高电平，0 为低电平
void SCl_Set(u8 flag);
/// @brief 控制数据线高低电平变化
/// @param flag 1 为高电平，0 为低电平
void SDA_Set(u8 flag);
/// @brief I2c 延时控制函数
void I2C_Delay(void);
/// @brief 设置 SDA 为开漏输出模式
void SDA_Output_Mode(void);
/// @brief 设置 SDA 为浮空输入模式
void SDA_Input_Mode(void);
/// @brief 发送 I2c 起始信号
///        1. scl 和 sda 都处于稳定的高电平状态
///        2. 在 scl 处于高电平状态时，让 sda 从高电平跳为低电平
///        3. scl 起始信号用于总线变化
void I2C_Start(void);
/// @brief 发送 i2c 停止信号
///        1. scl 和 sda 都处于稳定的高电平状态
///        2. 在 scl 处于高电平状态时，让 sda 从低电平跳为高电平
///        3. 停止信号让两条线进入稳定高电平状态
void I2C_Stop(void);
/**
 * @brief 读取 I2C 从机应答信号
 *      1. 保证当前 SDA 处于输入模式
 *      2. 需要时钟线配合，读取 SCL 高电平期间的 SDA 电平
 * @return 返回值对应 I2C_ACK(0) 应答成功，I2C_NACK(1) 应答失败
 */
I2C_ACK_t I2C_ReadBit(void);
/**
 * @brief 向 I2C 从机发送 1 个字节数据，需要主机发送数据包
 *      1. 1 字节数据逐位发送
 *      2. 当前数据发送完之后接收从机应答信号
 *      以下为 I2C 数据发送要求
 *          1. 数据位为 1：SCL 处于稳定高电平状态时 SDA 处于稳定高电平
 *          2. 数据位为 0：SCL 处于稳定高电平状态时 SDA 处于稳定低电平
 *      【注意】所有 SDA 电平变化都必须在 SCL 低电平期间完成
 * @param data 用于发送的 1 字节数据内容
 * @return 从机应答信息
 */
u8 I2C_SendByte(u8 data);
/**
 * @brief 从 I2C 从机读取 1 个字节数据
 * @param ack 主机应答信号，读取完成后返回给从机
 * @return 读取到的 1 字节数据
 */
u8 I2C_ReadByte(I2C_ACK_t ack);
#endif
