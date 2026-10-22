#include "usart3.h"

/**
 * @brief USART3 模块初始化函数，要求用户提供对应工作波特率
 *			Tx ==> PB10 【复用推挽输出模式】
 *	        Rx ==> PB11	【浮空输入模式】
 *          NRZ 数据格式为 8N1 模式
 *
 * @param brr 用户提供的波特率参数，需要根据当前 MCU 时钟频率和
 *				进行合理选择
 */
void USART3_Init(u32 brr)
{
    // 1. RCC 时钟使能模块，包括 USART3 和 GPIOB
    RCC->APB2ENR |= RCC_APB2_GPIOB_CLOCK_ENABLE;
    RCC->APB1ENR |= RCC_APB1_USART3_CLOCK_ENABLE;

    /*
    2. GPIOB 对应 IO 引脚配置
        PB10 【复用推挽输出模式】
        PB11 【浮空输入模式】
    */
    GPIOB->CRH &= ~(0x0F << 8);
    GPIOB->CRH |= (Alternate_Push_And_Pull_Speed_2MHz << 8);

    GPIOB->CRH &= ~(0x0F << 12);
    GPIOB->CRH |= (Floating_Input_Mode << 12);

    // 【明确关闭 USART3】
    USART3->CR1 &= ~(0x01 << 13);
    /*
    3. USART3 配置
        3.1 8N1 工作模式
        3.2 波特率设置，【重点】当前 STM32F103ZET6 USART3 对应时钟分支
            时钟基础频率为 36 MHz
        3.3 注册当前 USART3 对应 RXNE 和 IDLE 中断
        3.4 使能 USART3 Tx 和 Rx
        3.5 使能 USART3 工作
    */
    //  3.1 8N1 工作模式
    USART3->CR1 &= ~(0x01 << 12); // 8 位数据位 1 位起始位 N 位停止位
    USART3->CR1 &= ~(0x01 << 10); // 0 位校验位，关闭数据校验
    USART3->CR2 &= ~(0x03 << 12); // 明确当前 1 位停止位

    /*
    3.2 波特率设置，【重点】当前 STM32F103ZET6 USART3 对应时钟分支
        时钟基础频率为 36 MHz
    */
    float usart_div = 36000000 / 16 / brr;

    u16 usart_mantissa = (u16)usart_div;
    u16 usart_fraction = (u16)((usart_div - usart_mantissa) * 16);

    USART3->BRR = usart_mantissa << 4 | usart_fraction;

    // 3.3 注册当前 USART3 对应 RXNE 和 IDLE 中断
    USART3->CR1 |= (0x01 << 5); // RXNE 使能
    USART3->CR1 |= (0x01 << 4); // IDLE 使能

    NVIC_SetPriority(USART3_IRQn, 1);
    NVIC_EnableIRQ(USART3_IRQn);

    // 3.4 使能 USART3 Tx 和 Rx
    USART3->CR1 |= (0x01 << 3); // 使能 Tx
    USART3->CR1 |= (0x01 << 2); // 使能 Rx

    // 3.5 使能 USART3 工作
    USART3->CR1 |= (0x01 << 13);
}

USART3_Data usart3_value = {0};

void USART3_IRQHandler(void)
{
    /*
    1. 数据接收缓冲区清除
    2. IDLE 中断处理
    3. RXNE 中断处理
    */
    if (usart3_value.flag)
    {
        memset(&usart3_value, 0, sizeof(USART3_Data));
    }

    // 2. IDLE 中断处理
    if (USART3->SR & (0x01 << 4))
    {
        USART3->DR;

        usart3_value.flag = 1;

        /*
        TODO 数据临时处理
            USART3 目前连接的是 WiFi 模块
            USART1 是连接到 PC 端，可以在串口调试助手看到数据
        */
        USART1_SendString("USART3 RX : ");
        USART1_SendBuffer(usart3_value.data, usart3_value.count);
        USART1_SendString("\r\n");

        // 当前数据进行判断
        if (strstr(usart3_value.data + 3, "attributes"))
        {
            if (strstr(usart3_value.data + 3, "\"Beep\":true"))
            {
                Beep_Ctrl(1);
            }
            else if (strstr(usart3_value.data + 3, "\"Beep\":false"))
            {
                Beep_Ctrl(0);
            }
        }
    }

    // 3. RXNE 中断处理
    if (USART3->SR & (0x01 << 5))
    {
        usart3_value.data[usart3_value.count++] = USART3->DR;

        if (USART3_BUFFER_SIZE == usart3_value.count)
        {
            usart3_value.flag = 1;
            /*
            TODO 数据临时处理
                USART3 目前连接的是 WiFi 模块
                USART1 是连接到 PC 端，可以在串口调试助手看到数据
            */
            USART1_SendString("USART3 RX : ");
            USART1_SendBuffer(usart3_value.data, usart3_value.count);
            USART1_SendString("\r\n");
        }
    }
}

/**
 * @brief USART3 模块发送一个字节数据到接收端
 *
 * @param byte 需要进行发送的一个字节数据
 */
void USART3_SendByte(u8 byte)
{
    while (!(USART3->SR & (0x01 << 6)))
        ;

    USART3->DR = byte;
}

/**
 * @brief USART3 模块指定字节个数数据内容到接收端
 *
 * @param buffer 发送数据对应内存空间首地址
 * @param len    发送目标数据字节数
 */
void USART3_SendBuffer(u8 *buffer, u16 len)
{
    while (len--)
    {
        USART3_SendByte(*buffer);
        buffer += 1;
    }
}

/**
 * @brief USART3 模块发送字符串数据到接收端
 *
 * @param str 发送的字符串数据内容
 */
void USART3_SendString(const char *str)
{
    while (*str)
    {
        USART3_SendByte(*str);
        str += 1;
    }
}
