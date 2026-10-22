#include "soft_i2c.h"

/**
 * @brief 需要完成针对于 I2C 模块初始化操作，要求配置
 *      PB6 ==> SCL 对应通用推挽输出模式，速率 2 MHz
 *      PB7 ==> SDA 对应通用推挽输出模式，速率 2 MHz
 */
void I2C_GPIO_Init(void)
{
    // 1. RCC 时钟使能 GPIOB
    RCC->APB2ENR |= RCC_APB2_GPIOB_CLOCK_ENABLE;

    // 2. 配置 PB6 和 PB7 对应通用推挽输出模式，速率 2 MHz
    GPIOB->CRL &= ~(0x0F << 24);
    GPIOB->CRL |= General_Open_Drain_Output_Speed_2MHz << 24;

    GPIOB->CRL &= ~(0x0F << 28);
    GPIOB->CRL |= General_Open_Drain_Output_Speed_2MHz << 28;

    // 3. 利用 ODR 寄存器控制当前 GPIO 引脚开漏模式输出高电平
    // N-MOS 截止，对应 I2C 数据总线空闲状态。
    GPIOB->ODR |= I2C_SCL_PIN;
    GPIOB->ODR |= I2C_SDA_PIN;
}

/**
 * @brief 控制 SCL 时钟线输出电平情况
 *
 * @param flag 用户提供 1 引脚输出高电平，用户提供 0 引脚输出低电平
 */
void SCL_Set(u8 flag)
{
    if (flag)
    {
        GPIOB->ODR |= I2C_SCL_PIN;
    }
    else
    {
        GPIOB->ODR &= ~(I2C_SCL_PIN);
    }
}

/**
 * @brief 控制 SDA 数据线输出电平情况
 *
 * @param flag 用户提供 1 引脚输出高电平，用户提供 0 引脚输出低电平
 */
void SDA_Set(u8 flag)
{
    if (flag)
    {
        GPIOB->ODR |= I2C_SDA_PIN;
    }
    else
    {
        GPIOB->ODR &= ~(I2C_SDA_PIN);
    }
}

/**
 * @brief I2C 延时控制函数，主要用于 SCL 时钟周期控制
 */
void I2C_Delay(void)
{
    delay_us(I2C_DELAY_TIME);
}

/**
 * @brief 设置 SDA 对应引脚为输出模式，对应【通用开漏输出模式 2 MHz】
 */
void SDA_Output_Mode(void)
{
    GPIOB->CRL &= ~(0x0F << 28);
    GPIOB->CRL |= General_Open_Drain_Output_Speed_2MHz << 28;
}

/**
 * @brief 设置 SDA 对应引脚为输入模式，对应【浮空输入模式】，当前 GPIO 引脚电平状态
 *      完全取决于从机设备提供的电平信息。
 */
void SDA_Input_Mode(void)
{
    GPIOB->CRL &= ~(0x0F << 28);
    GPIOB->CRL |= Floating_Input_Mode << 28;
}

/**
 * @brief 【软件 I2C】起始信号，控制 SCL 和 SDA 实现 I2C 起始信号
 *      电气层面电平要求
 *      1. SCL 处于稳定的高电平状态
 *      2. SDA 在 SCL 高电平状态下，完成一次高电平，到低电平跳变过程
 *      3. SCL 进入后续的周期性电平变化过程，提供时钟周期
 */
void I2C_Start(void)
{
    // 1. 【重点】必须保证 SDA 数据工作模式为输出模式
    SDA_Output_Mode();

    // 2. 【I2C 空闲状态过程】SDA 和 SCL 同时处于高电平状态
    SCL_Set(1);
    SDA_Set(1);
    I2C_Delay();

    // 3. 【I2C 起始信号】SDA 高电平跳变操作
    SDA_Set(0);
    I2C_Delay();

    // 4. SCL 进入低电平无效时钟周期，准备后续的周期性时钟提供
    SCL_Set(0);
    I2C_Delay();
}

/**
 * @brief 【软件 I2C】停止信号，控制 SCL 和 SDA 实现 I2C 停止信号
 *      电气层面电平要求
 *      1. SCL 处于稳定的高电平状态
 *      2. SDA 在 SCL 高电平状态下，完成一次低电平，到高电平跳变过程
 *      3. 后续 SCL 和 SDA 进入稳定高电平状态 ==> 【I2C 空闲状态】
 */
void I2C_Stop(void)
{
    // 1. 【重点】必须保证 SDA 数据工作模式为输出模式
    SDA_Output_Mode();

    // 2. 【目前处于应答结束状态】SCL 要求为低电平状态，SDA 处于低电平状态
    SCL_Set(0);
    SDA_Set(0);
    I2C_Delay();

    // 2. 【SCL 时钟线】进入高电平状态，进入时钟有效周期
    SCL_Set(1);
    I2C_Delay();

    // 3. 【SDA 数据线】从低电平到高电平完成跳变
    SDA_Set(1);
    I2C_Delay();

    /*
    此时 SDA 和 SCL 都处于高电平状态，对应【I2C 空闲状态】
    */
}

/**
 * @brief 【软件 I2C】用于主机读应答数据内容
 *      1. 保证当前 SDA 处于输入模式
 *      2. 需要其他程序配合，控制 SCL 进行周期性电平变化。
 *
 * @return 返回值对应 I2C_ACK(0) 应答成功，I2C_NACK(1) 应答失败
 */
I2C_ACK_t I2C_ReadBit(void)
{
    /*
    PB7 对应引脚 IDR 数据判断
        如果 IDR 为高电平，返回结果对应 应答失败，I2C_NACK
        如果 IDR 为低电平，返回结果对应 应答成功，I2C_ACK
    */
    return GPIOB->IDR & I2C_SDA_PIN ? I2C_NACK : I2C_ACK;
}

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
u8 I2C_SendByte(u8 data)
{
    // 1. 【重点】必须保证 SDA 数据工作模式为输出模式
    SDA_Output_Mode();

    // 2. 准备循环控制变量和对应 I2C ACK 应答信息变量
    u8 i = 0;
    I2C_ACK_t ack = I2C_ACK;

    // 3. 利用 for 循环进行数据发送，需要控制 SCL 和 SDA
    for (i = 0; i < 8; i++)
    {
        // 3.1 SCL 处于低电平状态，进入无效时钟周期
        SCL_Set(0);
        I2C_Delay();

        // 3.2 控制 SDA 进行数据发送
        /*
        假设数据为 0110 0011 根据 if 判断
                0110 0011
             &  1000 0000
                0000 0000 ==> 对应当前数据最高位为 0，需要 SDA 低电平，发送数据 0

        假设数据为 1010 0011 根据 if 判断
                1010 0011
             &  1000 0000
                1000 0000 ==> 对应当前数据最高位为 1，需要 SDA 高电平，发送数据 1
        */
        if (data & 0x80)
        {
            SDA_Set(1);
        }
        else
        {
            SDA_Set(0);
        }
        I2C_Delay();

        // 3.3 SCL 进入高电平有效时钟周期，将 SDA 数据进行发送
        SCL_Set(1);
        I2C_Delay();

        // 3.3 保证 data 发送完成一位之后，进行左移操作
        data <<= 1;
    }

    /*
    以上操作是将 8 位数据发送到从机，此时需要处理从机应答数据。
    */
    // 数据发送完成之后，SCL 处于高电平状态，需要将 SCL 拉低处理，满足时钟周期需求
    SCL_Set(0);
    I2C_Delay();

    // 转换当前 SDA 从输出模式改为输入模式
    SDA_Input_Mode();
    I2C_Delay();

    // 需要将 SCL 拉高，进入有效电平周期，告知从机可以进行数据发送
    SCL_Set(1);
    I2C_Delay();

    // 在 SCL 高电平稳定状态下，读取 SDA 电平数据，判断从机应答情况下
    ack = I2C_ReadBit();

    /*
    保证 SCL 时序完整
     */
    SCL_Set(0);
    I2C_Delay();

    return ack;
}

/**
 * @brief 【软件 I2C】主机读取从机发送的 1 个字节数据内容，同时设置当前主机收到数据之后
 *      应答信息，用于告知从机是否继续发送数据。
 *
 * @param ack 用户提供参数为 I2C_ACK(0), 告知从机继续发送数据。
 *              用户提供参数为 I2C_NACK(1), 告知从机停止发送数据。
 * @return 对应当前读取到的 1 字节数据内容。
 */
u8 I2C_ReadByte(I2C_ACK_t ack)
{
    // 1. 【重点】必须保证 SDA 数据工作模式为输入模式
    SDA_Input_Mode();

    // 2. 循环控制变量和数据存储变量
    u8 i = 0;
    u8 data = 0;

    // 3. 利用 for 循环开始读取从机发送到主机的数据
    for (i = 0; i < 8; i++)
    {
        /*
        3.1 SCL 拉低进入无效时钟电平周期
        */
        SCL_Set(0);
        I2C_Delay();

        /*
        3.2 SCL 拉高进入有效时钟电平周期，同时告知从机进行数据发送
        */
        SCL_Set(1);
        I2C_Delay();

        /*
        3.3 用于存储数据的临时变量 data 在每一次接收数据之前进行
            左移操作。data 低位会自动补 0
        */
        data <<= 1;
        /*
        通过 I2C_ReadBit() 读取到数据为 1，表示当前从机发送数据为 1
        将数据通过 |= 存储到 data 变量中，如果读取到 0，if 大括号内容
        不再执行
        */
        if (I2C_ReadBit())
        {
            data |= 1;
        }
    }

    // 4. 在接收数据完成之后，SCL 处于高电平状态，将 SCL 电平拉低，保证时钟周期完整性
    SCL_Set(0);
    I2C_Delay();

    /*
    5. 主机需要发送应答信息到从机，需要将 SDA 数据线工作模式转换为输出模式。
    */
    SDA_Output_Mode();

    /*
    6. 根据用户提供的参数 ack 调整 SDA 电平情况
    */
    SDA_Set(ack);
    I2C_Delay();

    /*
    7. SCL 时钟线拉高，将当前主机应答 ACK 数据发送到从机
    */
    SCL_Set(1);
    I2C_Delay();

    /*
    保证 SCL 时序完整
    */
    SCL_Set(0);
    I2C_Delay();

    return data;
}
