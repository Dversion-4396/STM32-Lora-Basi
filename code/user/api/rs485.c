#include "rs485.h"

/**
 * @brief RS485 模块初始化函数，要求提供的参数为对应 485 
 *      传输速率对应波特率。需要赋值给 USART 模块
 * 
 * @param brr 当前 485 模块传输速率
 */
void RS485_Init(u16 brr)
{
    /*
    1. MCU 是利用 USART2 模块和 485 进行通信。对 USART2 模块进行
        初始化操作
    2. 485 芯片工作模式通过 PD7 引脚进行控制。时钟使能 GPIOD，同时
        配置 PD7 引脚工作模式【通用推挽输出模式】
    */

    // 1. USART2 模块初始化
    USART2_Init(brr);

    // 2. PD7 引脚配置
    // 2.1 GPIOD 时钟使能
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOD, ENABLE);

    // 2.2 GPIO 工作模式初始化
    GPIO_InitTypeDef GPIO_InitStructure = {0};

    GPIO_InitStructure.GPIO_Pin = RS485_RE_DE_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;

    GPIO_Init(RS485_RE_DE_PIN_GROUP, &GPIO_InitStructure);

    // 2.3 PD7 引脚默认电平为低电平，485 芯片处于接收数据状态。
    /*
    void GPIO_ResetBits(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin);
        设置指定 GPIO 引脚低电平状态。
    */
    GPIO_ResetBits(RS485_RE_DE_PIN_GROUP, RS485_RE_DE_PIN);
}

/**
 * @brief RS485 模块数据发送函数，需要通过控制 PD7 引脚电平来切换 485 
 *      芯片工作模式
 *      PD7 高电平 ==》 RE = 1 DE = 1 485 芯片通过 AB 端子对外发送数据
 *      PD7 低电平 ==》 RE = 0 DE = 0 485 芯片接收 AB 端子传递数据。
 * 
 * @param data  需要发送数据内存地址
 * @param count 需要发送的数据字节数
 */
void RS485_SendBuffer(u8 *data, u16 count)
{
    // 1. 切换控制当前 485 芯片工作模式引脚为高电平状态【485 发送数据模式】
    GPIO_SetBits(RS485_RE_DE_PIN_GROUP, RS485_RE_DE_PIN);
    
    // 2. 利用 USART2 模块进行数据发送
    USART2_SendBuffer(data, count);

    // 3. 判断当前 USART2 对应的 TC 标志位是否完成发送，
    // 如果完成 485 芯片转入接收模式
    while (!USART_GetFlagStatus(USART2, USART_FLAG_TC));

    // 4. 切换控制当前 485 芯片工作模式引脚为低电平状态【485 接收数据模式】
    GPIO_ResetBits(RS485_RE_DE_PIN_GROUP, RS485_RE_DE_PIN);
}