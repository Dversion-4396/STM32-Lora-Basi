/**
  ******************************************************************************
  * @file    raindrop.h
  * @brief   雨滴传感器驱动头文件（STM32F10x 标准外设库）
  * @note    同时支持 DO 数字量（LM393 比较器输出）与 AO 模拟量（ADC 采样）
  ******************************************************************************
  */

#ifndef __RAINDROP_H
#define __RAINDROP_H

#include "common.h"

/* 串口输出语言控制：1 = 中文，0 = 英文
 * 串口助手若显示中文为乱码，将下面的 1 改成 0 即可切换为英文输出 */
#ifndef RAIN_PRINT_CN
#define RAIN_PRINT_CN (1)
#endif

/* 雨量等级：无雨 / 小雨 / 中雨 / 大雨 */
typedef enum
{
    RAIN_NONE   = 0,    /* 无雨（干燥） */
    RAIN_SMALL  = 1,    /* 小雨         */
    RAIN_MIDDLE = 2,    /* 中雨         */
    RAIN_BIG    = 3     /* 大雨         */
} RainLevel_t;

/* 雨滴传感器一次采集的完整数据 */
typedef struct
{
    uint8_t     doLevel;    /* DO 原始电平：1 = 高电平（无水），0 = 低电平（有水） */
    uint8_t     isWet;      /* 1 = 检测到雨水，0 = 干燥 */
    uint16_t    adcRaw;     /* AO 滤波后的 ADC 原始值（0~4095） */
    uint16_t    voltageMv;  /* AO 换算电压，mV */
    uint8_t     wetPercent; /* 湿润百分比，0~100，越大越湿 */
    RainLevel_t level;      /* 雨水等级 */
    /*
     * ADC 有效性标志：
     *   1 = ADC 读数正常（有正常抖动）
     *   0 = ADC 读数异常（长时间纹丝不动且贴边 0/4095），
     *       通常说明 AO 引脚悬空或 AO 线没接，此时雨量等级不可信。
     */
    uint8_t     adcValid;
} RainData_t;

/* ===================== 初始化接口（引脚初始化单独封装） ===================== */

/* DO 引脚单独初始化 */
void RainDrop_DO_GPIO_Init(void);
/* AO 引脚 + ADC1 单独初始化 */
void RainDrop_AO_GPIO_Init(void);
/* 传感器全部引脚初始化（内部依次调用上面两个） */
void RainDrop_GPIO_Init(void);

/* ===================== 数据采集接口 ===================== */

/* 读取 DO 原始电平：返回 1 = 高（无水），0 = 低（有水） */
uint8_t  RainDrop_ReadDO_Level(void);
/* 读取 DO 并做消抖：返回 1 = 高（无水），0 = 低（有水） */
uint8_t  RainDrop_ReadDO_LevelStable(void);

/* AO 单次 ADC 采样 */
uint16_t RainDrop_ReadAO_Raw(void);
/* AO 多次采样求平均（均值滤波） */
uint16_t RainDrop_ReadAO_Average(uint8_t times);
/* ADC 值 -> 电压(mV) */
uint16_t RainDrop_ConvertToVoltage(uint16_t adcValue);
/* ADC 值 -> 湿润百分比(0~100) */
uint8_t  RainDrop_ConvertToWetPercent(uint16_t adcValue);
/* ADC 原始值 -> 雨量等级（无 / 小 / 中 / 大） */
RainLevel_t RainDrop_GetLevel(uint16_t adcRaw);

/* 一次性采集全部数据 */
void RainDrop_GetData(RainData_t *pData);

/* 雨量等级转字符串，方便串口打印 */
const char *RainDrop_LevelString(RainLevel_t level);

/* DO 是否有水转字符串，方便串口打印 */
const char *RainDrop_WaterString(uint8_t isWet);

#endif /* __RAINDROP_H */
