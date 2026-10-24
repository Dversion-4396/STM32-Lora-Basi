#ifndef _LORA_AS32_H
#define _LORA_AS32_H

#include "common.h"
// LoRa 模块数据收发通过 USART3 完成
#include "usart3.h"

/*
【模块说明】
	当前驱动适配泽耀 AS32-TTL-100-C LoRa 无线串口模块（470MHz 频段版本）
	手册依据：
		1. 《as32ttl100ccn.pdf》          产品规格书
		2. 《AS32系列串口命令说明.pdf》   命令帧格式 + 参数寄存器位定义
		3. 《AS32-TTL系列操作指南.pdf》   接线方式 + 电平说明
		4. 《注意事项-20220919.pdf》      供电、天线、ESD 注意事项
*/

/*
【引脚分配说明】模块引脚定义详见规格书 5.2 节
	LoRa 模块引脚       STM32F103ZET6          说明
	RXD          <==    PB10 (USART3_TX)       模块串口接收
	TXD          ==>    PB11 (USART3_RX)       模块串口发送
	MD0          <==    PB12                   模式选择，模块内部上拉，MCU 开漏输出
	MD1          <==    PB13                   模式选择，模块内部上拉，MCU 开漏输出
	AUX          ==>    PB14                   模块状态输出，MCU 浮空输入
	VCC          <==    3.3V ~ 5V              供电范围 2.5~5.5V，需预留 200mA 瞬态电流
	GND          <==    GND
*/

/*
AS32-TTL-100-C 模块工作状态宏定义，详见规格书工作状态表
	工作状态编码 bit0 对应 MD0 电平，bit1 对应 MD1 电平
*/
#define LORA_MODE_NORMAL     (0x00) // 一般工作状态  MD0 = 0  MD1 = 0，透明传输
#define LORA_MODE_WAKE_UP    (0x01) // 唤醒工作状态  MD0 = 1  MD1 = 0，发射前自动加唤醒码
#define LORA_MODE_POWER_SAVE (0x02) // 省电工作状态  MD0 = 0  MD1 = 1，空中唤醒接收
#define LORA_MODE_SLEEP      (0x03) // 休眠工作状态  MD0 = 1  MD1 = 1，参数配置

/*
模块命令字宏定义，详见《AS32系列串口命令说明.pdf》命令概述表
【重点】命令只有在休眠工作状态（MD0 = 1，MD1 = 1）下才有效，
		且休眠工作状态下串口固定为 9600bps 8N1
*/
#define LORA_CMD_SET_CONFIG    (0xC0) // 设置模块参数，配置的参数断电后可保存
#define LORA_CMD_READ_CONFIG   (0xC1) // 读取模块配置参数，需要连续发送 3 次
#define LORA_CMD_READ_VERSION  (0xC3) // 读取模块的硬件版本号，需要连续发送 3 次
#define LORA_CMD_RESET         (0xC4) // 复位模块命令，需要连续发送 3 次
#define LORA_CMD_READ_VOLTAGE  (0xC5) // 读取模块的实际电压，需要连续发送 3 次
#define LORA_CMD_RESTORE       (0xC9) // 恢复默认参数配置，需要连续发送 3 次

/*
SPEED 通信配置寄存器常用宏定义，详见命令说明 6.2 节
	SPEED[7:6] 串口校验位    SPEED[5:3] 串口波特率    SPEED[2:0] 空中速率
	出厂默认 0x1A = 8N1 + 9600bps + 2.4k
*/
#define LORA_UART_8N1          (0x00)       // 串口校验位 8N1（默认）
#define LORA_UART_BAUD_9600    (0x03 << 3)  // 串口波特率 9600bps（默认）
#define LORA_UART_BAUD_115200  (0x07 << 3)  // 串口波特率 115200bps
#define LORA_AIR_SPEED_2K4     (0x02)       // 空中速率 2.4k（默认）
#define LORA_AIR_SPEED_9K6     (0x04)       // 空中速率 9.6k
#define LORA_AIR_SPEED_19K2    (0x05)       // 空中速率 19.2k

/*
OPTION 特殊功能寄存器常用宏定义，详见命令说明 6.2 节
	OPTION[7] 定点发送使能    OPTION[6] IO 口驱动方式
	出厂默认 0x40 = 透明传输 + 推挽输出
*/
#define LORA_OPTION_TRANSPARENT (0x40) // 透明传输（默认）
#define LORA_OPTION_FIXED       (0xC0) // 定点传输

/*
模块出厂默认信道，通信频率 = 410 + CHAN * 1M
【重点】AS32-TTL-100-C 出厂信道 0x3D 对应 471MHz，
		与 AS32-TTL-100 出厂 0x17（433MHz）不同，通信双方信道必须一致
*/
#define LORA_DEFAULT_CHAN (0x3D)

/*
AS32-TTL-100-C 模块参数结构体
	对应参数配置命令 0xC0 ADDH ADDL SPEED CHAN OPTION 寄存器内容
*/
typedef struct lora_config
{
	u8 addh;   // 模块地址高 8 位
	u8 addl;   // 模块地址低 8 位
	u8 speed;  // 通信配置寄存器（串口校验 + 串口波特率 + 空中速率）
	u8 chan;   // 通信信道寄存器，通信频率 = 410 + CHAN * 1M
	u8 option; // 特殊功能寄存器（定点使能 + IO 驱动方式 + 唤醒时间）
} LORA_Config;

/**
 * @brief LoRa 模块初始化函数，完成 USART3、MD0、MD1、AUX 引脚配置，
 *			初始化完成后模块进入一般工作状态（透明传输）
 *
 * @param brr 模块串口工作波特率，出厂默认 9600bps，
 *				需要与模块 SPEED 寄存器配置的串口波特率保持一致
 */
void LORA_Init(u32 brr);

/**
 * @brief 设置 LoRa 模块工作状态（MD0、MD1 引脚电平组合）
 *
 * @param mode 工作状态编码，对应 LORA_MODE_XXX 宏定义
 */
void LORA_SetMode(u8 mode);

/**
 * @brief 获取模块 AUX 引脚当前电平状态
 *
 * @return 1: AUX 高电平，模块空闲  0: AUX 低电平，模块繁忙
 */
u8 LORA_GetAUX(void);

/**
 * @brief 设置模块配置参数（0xC0 命令，配置的参数断电后可保存）
 *
 * @param config 模块参数结构体指针
 * @return 1: 配置成功（模块应答 OK）  0: 配置失败
 */
u8 LORA_SetConfig(const LORA_Config *config);

/**
 * @brief 读取模块当前配置参数（0xC1 命令）
 *
 * @param config 模块参数结构体指针，用于存放读取结果
 * @return 1: 读取成功  0: 读取失败
 */
u8 LORA_ReadConfig(LORA_Config *config);

/**
 * @brief 复位模块（0xC4 命令）
 *
 * @return 1: 复位成功  0: 复位失败
 */
u8 LORA_Reset(void);

/**
 * @brief 读取模块的实际电压（0xC5 命令）
 *
 * @param voltage 用于存放电压值，单位 mV，例如 3100 表示 3.1V
 * @return 1: 读取成功  0: 读取失败
 */
u8 LORA_ReadVoltage(u16 *voltage);

/**
 * @brief 读取模块硬件版本号（0xC3 命令）
 *
 * @param version 用于存放版本号字符串的缓冲区
 * @param max_len 缓冲区总字节数（需 >= 15）
 * @return 1: 读取成功  0: 读取失败
 */
u8 LORA_ReadVersion(char *version, u16 max_len);

/**
 * @brief 透明传输模式发送数据
 *			【重点】模块内部具有 256 字节缓存 FIFO，单次发送不要超过 256 字节
 *
 * @param buffer 发送数据对应内存空间首地址
 * @param len    发送目标数据字节数
 */
void LORA_SendData(u8 *buffer, u16 len);

/**
 * @brief 定点传输模式发送数据
 *			【重点】需要先将 OPTION[7] 配置为 1 开启定点传输模式，
 *			发送方与接收方的地址、信道可以不相同
 *
 * @param address 接收方模块地址（0x0000 ~ 0xFFFF）
 * @param chan    接收方模块信道
 * @param buffer  发送数据对应内存空间首地址
 * @param len     发送目标数据字节数
 */
void LORA_FixedSend(u16 address, u8 chan, u8 *buffer, u16 len);

#endif
