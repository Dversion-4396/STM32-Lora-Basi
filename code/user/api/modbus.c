#include "modbus.h"

/**
 * @brief Modbus 数据接收控制定时，使用 TIM7 基础定时实现。
 */
void Modbus_TIM_Config(void)
{
    // 1. RCC APB1 TIM7 定时器时钟使能
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM7, ENABLE);

    // 2. 【明确关闭定时器】保持 TIM7 相关配置可以完成，开启状态下
    // 部分配置无法完成
    TIM_Cmd(TIM7, DISABLE);

    // 3. 定时器配置初始化
    /*
    typedef struct
    {
        // PSC CNT计数器预分频器倍数
        uint16_t TIM_Prescaler;
        // CNT(Counter) 工作模式
        uint16_t TIM_CounterMode;
        // ARR 自动重装载寄存器
        uint16_t TIM_Period;
        // 当前系统时钟分配给当前定时器预分频倍数
        uint16_t TIM_ClockDivision;
        // 当前定时器是否重复, 基础定时没有当前功能，默认为 0
        uint8_t TIM_RepetitionCounter;
    } TIM_TimeBaseInitTypeDef;
    */
    TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure = {0};
    TIM_TimeBaseInitStructure.TIM_Prescaler = 72 - 1;
    TIM_TimeBaseInitStructure.TIM_Period = DATA_FRAME_TIME_INTERVAL - 1;
    TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;

    TIM_TimeBaseInit(TIM7, &TIM_TimeBaseInitStructure);

    // 设置当前定时为一次性定时。
    TIM_SelectOnePulseMode(TIM7, TIM_OPMode_Single);

    // 4. 配置当前定时器中断
    TIM_ITConfig(TIM7, TIM_IT_Update, ENABLE);

    NVIC_InitTypeDef NVIC_InitStructure = {0};
    NVIC_InitStructure.NVIC_IRQChannel = TIM7_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;

    NVIC_Init(&NVIC_InitStructure);

    // 5. 【注意】当前定时不开启！！！明确 Counter 计数器为 0
    TIM_SetCounter(TIM7, 0);
}

/*
基础定时器 TIM7 对应的中断请求处理函数
    如果当前中断触发，表示 Modbus 帧间隔时间超出 T3.5 T4.0 时间
    表示从机发送 Modbus 数据帧完毕。
    1. 定时器关闭
        1.1 如果是周期性定时，需要明确关闭
        1.2 如果是一次性定时器，可以缺省当前关闭操作
    2. CNT 计数器清零
    3. 对 USART2 数据接收缓冲区数据内容进行处理。对应 Modbus
        协议数据帧内容。
    4. 根据 Modbus 协议内容分析，如果接收数据小于 5 个字节，对应
        Modbus 数据帧非完整格式，错误数据，需要舍去。
    5. 其他数据进行后续的解析操作。
*/
void TIM7_IRQHandler(void)
{
    // 判断当前定时器是否触发更新中断 Update
    if (TIM_GetITStatus(TIM7, TIM_IT_Update))
    {
        // 清除中断标志位
        TIM7->SR = 0;

        // 明确定时器关闭，同时计数器数据清理
        TIM_Cmd(TIM7, DISABLE);
        TIM_SetCounter(TIM7, 0);

        // 判断有效数据字节数
        if (usart2_value.count < 5)
        {
            memset(&usart2_value, 0, sizeof(USART2_Data));
        }
        else
        {
            // 表示数据接收成功
            usart2_value.flag = 1;

            // TODO 临时展示目前收到的 Modbus 数据内容
            USART1_SendBuffer(usart2_value.data, usart2_value.count);

            // TODO 对当前 Modbus 数据进行解析操作。
            Modbus_Analysis_Temp_And_Humi(usart2_value.data, usart2_value.count);
        }
    }
}

/**
 * @brief 发送 Modbus 03 功能码对设备进行多个寄存器数据提取操作
 *
 * @param device_addr Modbus 从机设备设备地址
 * @param reg_addr    Modbus 从机设备对应读取寄存器起始地址
 * @param reg_count   期望读取的寄存器数量
 */
void Modbus_Send_03_Cmd(u8 device_addr, u16 reg_addr, u16 reg_count)
{
    /*
    Modbus 03 功能码问询帧对应的字节数固定位 8 个字节
        地址域 1 字节
        功能码 1 字节
        寄存器地址 2 字节
        寄存器数量 2 字节
        CRC 校验 2 字节
    */
    u8 modbus_03_cmd[8] = {0};
    u8 pos = 0;

    // 1. Modbus 设备地址 ==> 地址域
    modbus_03_cmd[pos++] = device_addr;

    // 2. 03 功能码
    modbus_03_cmd[pos++] = 0x03;

    // 3. 寄存器地址
    modbus_03_cmd[pos++] = reg_addr >> 8;
    modbus_03_cmd[pos++] = reg_addr & (0x00FF);

    // 4. 寄存器数量
    modbus_03_cmd[pos++] = reg_count >> 8;
    modbus_03_cmd[pos++] = reg_count & (0x00FF);

    // 5. 寄存器 CRC 数据
    u16 crc_value = Modbus_CRC16(modbus_03_cmd, pos);

    // Modbus CRC 数据低位在前，高位在后
    modbus_03_cmd[pos++] = crc_value & (0x00FF);
    modbus_03_cmd[pos++] = crc_value >> 8;

    // 6. 利用 RS485 模块将数据发送给从机
    RS485_SendBuffer(modbus_03_cmd, pos);
}
/*
01 03 0A 01 C4 01 05 02 32 00 23 00 3C 4A 87
*/
/*
数据解析函数是针对于不同的功能码信息处理
*/
u8 Modbus_Analysis_Temp_And_Humi(u8 *buffer, u16 count)
{
    u8 modbus_data[] = {
        0x01,
        0x03,
        0x0A,
        0x01,
        0xC4,
        0x01,
        0x05,
        0x02,
        0x32,
        0x00,
        0x23,
        0x00,
        0x3C,
        0x4A,
        0x87,
    };
}

/**
 * @brief Modbus CRC-16 计算函数
 *
 * @param data   待计算的数据指针
 * @param length 数据长度（字节数）
 * @return CRC校验值（16位）
 */
uint16_t Modbus_CRC16(const uint8_t *data, size_t length)
{
    uint16_t crc = 0xFFFF; // 初始值
    size_t i, j;

    for (i = 0; i < length; i++)
    {
        crc ^= data[i]; // 与当前字节异或

        for (j = 0; j < 8; j++)
        {
            if (crc & 0x0001)
            { // 检查最低位
                crc >>= 1;
                crc ^= 0xA001; // 多项式0x8005的逆序
            }
            else
            {
                crc >>= 1;
            }
        }
    }

    return crc; // 返回CRC值（无需取反）
}