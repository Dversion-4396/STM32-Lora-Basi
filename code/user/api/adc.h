#ifndef _ADC_H
#define _ADC_H

#include "common.h"

extern u16 lsen_value;

/**
 * @brief ADC3 模块初始化，需要完成
 *      1. 时钟使能 ADC3 模块和 GPIOF 模块
 *      2. PF8 配置为【模拟输入模式】
 *      3. ADC 相关配置
 */
void ADC3_Init(void);

/**
 * @brief 获取光敏电阻数据
 * 
 * @return 返回光敏电阻对应 ADC 值
 */
u16 Get_Lsen_Value(void);

#endif
