#ifndef _EEPROM_AT24C02_H
#define _EEPROM_AT24C02_H

#include "common.h"
#include "soft_i2c.h"

/*
设备真实 7 位地址对应 101 0000
*/
#define EEPROM_AT24C02_ADDR (0x50)

/*
因为 I2C 数据传递不稳定，有可能需要多次尝试数据传递，设置最大尝试次数
*/
#define I2C_RETRY_COUNT (3)

/*
I2C 设备响应对应超时时间，时间为 10 ms
*/
#define I2C_TIMEOUT (10)

/*
读写标志位枚举
*/
typedef enum
{
    WRITE_FLAG,
    READ_FLAG
} I2C_RW_Flag_t;

/**
 * @brief 写入一个字节数据到 EEPROM AT24C02 存储芯片中，
 *
 * @param eeprom_addr EEPROM AT24C02 存储芯片 I2C 设备地址
 * @param reg_addr    目标写入到存储芯片寄存器地址
 * @param data        目标写入的 1 字节数据内容
 * @return 对应当前从机应答信息。
 */
u8 EEPROM_WriteByte(u8 eeprom_addr, u8 reg_addr, u8 data);

/**
 * @brief 从 EEPROM AT24C02 存储芯片中读取一个字节数据
 *
 * @param eeprom_addr EEPROM AT24C02 存储芯片 I2C 设备地址
 * @param reg_addr    目标读取数据的寄存器地址
 * @return 对应读取到的数据内容
 */
u8 EEPROM_ReadByte(u8 eeprom_addr, u8 reg_addr);

#endif
