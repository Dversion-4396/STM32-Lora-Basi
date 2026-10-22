#ifndef _SOFT_I2C_H
#define _SOFT_I2C_H

#include "common.h"

#include "delay.h"

// I2C 对应 GPIO 引脚编号和对应 GPIOB 分组
#define I2C_SCL_PIN    GPIO_Pin_6
#define I2C_SDA_PIN    GPIO_Pin_7
#define I2C_GPIO_GROUP GPIOB

// I2C 延时控制函数 5 us
#define I2C_DELAY_TIME (5)

typedef enum
{
    I2C_ACK,
    I2C_NACK
} I2C_ACK_t;

/**
 * @brief 需要完成针对于 I2C 模块初始化操作，要求配置
 *      PB6 ==> SCL 对应通用推挽输出模式，速率 2 MHz
 *      PB7 ==> SDA 对应通用推挽输出模式，速率 2 MHz
 */
void I2C_GPIO_Init(void);

/**
 * @brief 控制 SCL 时钟线输出电平情况
 *
 * @param flag 用户提供 1 引脚输出高电平，用户提供 0 引脚输出低电平
 */
void SCL_Set(u8 flag);

/**
 * @brief 控制 SDA 数据线输出电平情况
 *
 * @param flag 用户提供 1 引脚输出高电平，用户提供 0 引脚输出低电平
 */
void SDA_Set(u8 flag);

/**
 * @brief I2C 延时控制函数，主要用于 SCL 时钟周期控制
 */
void I2C_Delay(void);

/**
 * @brief 设置 SDA 对应引脚为输出模式，对应【通用开漏输出模式 2 MHz】
 */
void SDA_Output_Mode(void);

/**
 * @brief 设置 SDA 对应引脚为输入模式，对应【浮空输入模式】，当前 GPIO 引脚电平状态
 *      完全取决于从机设备提供的电平信息。
 */
void SDA_Input_Mode(void);

/**
 * @brief 【软件 I2C】起始信号，控制 SCL 和 SDA 实现 I2C 起始信号
 *      电气层面电平要求
 *      1. SCL 处于稳定的高电平状态
 *      2. SDA 在 SCL 高电平状态下，完成一次高电平，到低电平跳变过程
 *      3. SCL 进入后续的周期性电平变化过程，提供时钟周期
 */
void I2C_Start(void);

/**
 * @brief 【软件 I2C】停止信号，控制 SCL 和 SDA 实现 I2C 停止信号
 *      电气层面电平要求
 *      1. SCL 处于稳定的高电平状态
 *      2. SDA 在 SCL 高电平状态下，完成一次低电平，到高电平跳变过程
 *      3. 后续 SCL 和 SDA 进入稳定高电平状态 ==> 【I2C 空闲状态】
 */
void I2C_Stop(void);

/**
 * @brief 【软件 I2C】用于主机读应答数据内容
 *      1. 保证当前 SDA 处于输入模式
 *      2. 需要其他程序配合，控制 SCL 进行周期性电平变化。
 * 
 * @return 返回值对应 I2C_ACK(0) 应答成功，I2C_NACK(1) 应答失败
 */
I2C_ACK_t I2C_ReadBit(void);

/**
 * @brief 【软件 I2C】发送 1 个字节数据，需要处理的内容包括
 *      1. 1 字节数据内容
 *      2. 当前发送完成之后，从机的应答信号
 *      根据 I2C 数据发送要求
 *          1. 发送数据 1，SCL 处于稳定高电平状态，同时 SDA 处于稳定高电平
 *          2. 发送数据 0，SCL 处于稳定高电平状态，同时 SDA 处于稳定低电平
 *      【注意】所有的 SDA 电平变化，必须在 SCL 低电平周期内完成。
 * 
 * @param data 用于发送的 1 字节数据内容
 * @return 从机应答信息
 */
u8 I2C_SendByte(u8 data);

/**
 * @brief 【软件 I2C】主机读取从机发送的 1 个字节数据内容，同时设置当前主机收到数据之后
 *      应答信息，用于告知从机是否继续发送数据。
 *      
 * @param ack 用户提供参数为 I2C_ACK(0), 告知从机继续发送数据。
 *              用户提供参数为 I2C_NACK(1), 告知从机停止发送数据。
 * @return 对应当前读取到的 1 字节数据内容。
 */
u8 I2C_ReadByte(I2C_ACK_t ack);

#endif
