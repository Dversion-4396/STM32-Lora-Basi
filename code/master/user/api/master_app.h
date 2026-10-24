#ifndef _MASTER_APP_H
#define _MASTER_APP_H

#include "common.h"

/*
================================================================================
【模块说明】LoRa 主机：接收从机数据帧 -> 解析 -> OLED 显示
--------------------------------------------------------------------------------
    数据来源：从机通过 LoRa 透明传输发来的 ASCII 文本帧，格式为
        $RAIN,DHT20,T=27.5,H=51.1,ADC=1850,V=1490,WET=54,DO=1,LEVEL=1,WL=60*XX\r\n

    字段含义：
        T      温度，单位 ℃，保留一位小数；读取失败时是 "--"
        H      湿度，单位 %RH，保留一位小数；读取失败时是 "--"
        ADC    雨滴 AO 原始 ADC 值 0~4095
        V      雨滴 AO 电压 mV
        WET    雨滴湿润百分比 0~100
        DO     雨滴数字量 1=无水 0=有水
        LEVEL  雨情等级 0=无雨 1=小雨 2=中雨 3=大雨
        WL     水位百分比 0~100（本工程新加字段）
        *XX    校验和：对 "*" 之前所有字节累加取低 8 位，两位大写 HEX

    OLED 显示格式（4 行，每行占 2 页）：
        温度:26.3
        湿度:51.1
        水位:60
        无雨 / 小雨 / 中雨 / 大雨
================================================================================
*/

/*
 * 水位超高报警阈值（百分比）
 * 水位 > 此值时，蜂鸣器立即鸣叫 + OLED 立即显示“水位过高”（不遵循刷新周期）
 */
#define WATER_ALARM_THRESHOLD  (70)

/*
 * OLED 屏幕刷新周期（ms）
 * 【重点】数据每秒都会到，但屏幕不跟着每帧刷，
 * 只有到了这个周期才刷一次，避免 OLED 频繁刷新导致闪烁。
 */
#define OLED_REFRESH_MS        (10000)

/* 一帧解析结果 */
typedef struct
{
    float   temperature;   /* 温度 ℃ */
    float   humidity;      /* 湿度 %RH */
    uint8_t tempValid;     /* 1 = 温度有效（不是 "--"） */
    uint8_t humiValid;     /* 1 = 湿度有效 */
    uint16_t rainAdc;      /* 雨滴 ADC 原始值 */
    uint8_t  rainLevel;    /* 雨情等级 0~3 */
    uint8_t  waterLevel;   /* 水位百分比 0~100 */
    uint8_t  waterValid;   /* 1 = 水位字段存在且有效 */
    uint8_t  hasData;      /* 1 = 至少成功收到过一帧 */
    uint8_t  waterAlarm;   /* 1 = 水位超过阈值，触发报警 */
} MasterData_t;

/* 主机全局数据，供其它模块引用 */
extern MasterData_t master_data;

/**
 * @brief 主机初始化：OLED 初始化 + 显示等待画面
 * @note  必须在 I2C_GPIO_Init() 之后调用
 */
void Master_Init(void);

/**
 * @brief 解析从机发来的一帧
 * @param frame 帧数据（不要求 NUL 结尾，靠 len 限定）
 * @param len   帧长度
 * @return 1 = 解析成功（帧头、校验和都正确）；0 = 失败
 */
uint8_t Master_ParseFrame(const uint8_t *frame, uint16_t len);

/**
 * @brief 把 master_data 的内容刷新到 OLED
 */
void Master_Display(void);

/**
 * @brief 显示"等待数据"画面（上电或长时间无数据时）
 */
void Master_ShowWaiting(void);

#endif
