#ifndef _DHT20_H_
#define _DHT20_H_

#include "common.h"

/* DHT20 温湿度传感器驱动（软件模拟 I2C，STM32F103ZET6）
 *
 * 接线：
 *      DHT20 引脚 1  VDD --- 3.3V
 *      DHT20 引脚 2  SDA --- PB7
 *      DHT20 引脚 3  GND --- GND
 *      DHT20 引脚 4  SCL --- PB6
 *
 * 用法：
 *      DHT20_Init();                           上电调用一次
 *      DHT20_Read(&humidity, &temperature);    每 2s 调用一次，返回 DHT20_OK 才算有效
 */

#define DHT20_GPIO_PORT        GPIOB
#define DHT20_GPIO_CLK         RCC_APB2Periph_GPIOB
#define DHT20_SCL_PIN          GPIO_Pin_6              /* SCL -> PB6 */
#define DHT20_SDA_PIN          GPIO_Pin_7              /* SDA -> PB7 */

#define DHT20_ADDR             0x38                    /* 7 位从机地址 */
#define DHT20_ADDR_W           0x70                    /* 0x38<<1，写 */
#define DHT20_ADDR_R           0x71                    /* 0x38<<1|1，读 */
#define DHT20_CMD_TRIGGER      0xAC                    /* 触发测量命令 */
#define DHT20_CMD_TRIGGER_P1   0x33                    /* 命令参数 1 */
#define DHT20_CMD_TRIGGER_P2   0x00                    /* 命令参数 2 */

#define DHT20_STATUS_BUSY      0x80                    /* Bit7：1=忙，0=空闲 */
#define DHT20_STATUS_CAL       0x18                    /* Bit3：1=已校准 */

#define DHT20_POWER_MS         5                       /* 上电 5ms 后才可拉高 SCL/SDA */
#define DHT20_STARTUP_MS       100                     /* 上电稳定时间 */
#define DHT20_TRIGGER_MS       10                      /* 触发测量前等待 */
#define DHT20_MEASURE_MS       80                      /* 触发后等待 */
#define DHT20_POLL_TIMES       20                      /* 忙状态最大轮询次数 */
#define DHT20_POLL_GAP_MS      5                       /* 轮询间隔 */
#define DHT20_CYCLE_MS         2000                    /* 建议测量周期 */

#define DHT20_OK               0
#define DHT20_ERR_TRIG         1                       /* 触发命令无应答 */
#define DHT20_ERR_STATUS       2                       /* 测量超时 */
#define DHT20_ERR_READ         3                       /* 读数据无应答 */
#define DHT20_ERR_CRC          4                       /* CRC 校验失败 */

void    DHT20_Delay_us(uint32_t t);                    /* 微秒级延时 */
void    DHT20_Delay_ms(uint32_t t);                    /* 毫秒级延时 */
void    DHT20_Init(void);                              /* 上电初始化 */
uint8_t DHT20_Read_Raw(uint32_t *hum_raw, uint32_t *temp_raw);   /* 读 20bit 原始值 */
uint8_t DHT20_Read(float *humidity, float *temperature);         /* 读温度(℃)/湿度(%RH) */

#endif
