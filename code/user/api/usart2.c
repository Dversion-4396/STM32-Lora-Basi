#include "usart2.h"

/**
 * @brief USART2 模块初始化函数，对应 GPIO 引脚为
 *          PA2 ==> USART2_TX
 *          PA3 ==> USART2_RX
 *
 * @param brr 用户提供的参数当前 USART2 模块波特率
 */
void USART2_Init(u32 brr)
{
    // 1. RCC 时钟使能模块 GPIOA 和 USART2
    /*
    【标准库函数】
        void RCC_APB1PeriphClockCmd(uint32_t RCC_APB1Periph, FunctionalState NewState)
        APB1 分支模块对应使能
            uint32_t RCC_APB1Periph 要求提供参数为当前 APB1 分支对应模块枚举类型
            FunctionalState NewState 是否开启当前模块状态，参数有 ENABLE 和 DISABLE

        void RCC_APB2PeriphClockCmd(uint32_t RCC_APB2Periph, FunctionalState NewState);
        APB2 分支模块对应使能，操作和 APB1 一致

    */
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

    // 2. GPIO 模块配置
    // 2.1 PA2 USART2 Tx 复用推挽输出模式

    /*
    GPIO_InitTypeDef GPIO 模块对应初始化结构体。
    typedef struct
    {
        uint16_t GPIO_Pin;             // 引脚编号
        GPIOSpeed_TypeDef GPIO_Speed;  // 速度要求，如果为输入模式，当前参数无效
        GPIOMode_TypeDef GPIO_Mode;    // GPIO 工作模式
    } GPIO_InitTypeDef;

    GPIOSpeed_TypeDef GPIO 速率枚举类型
        typedef enum
        {
            GPIO_Speed_10MHz = 1,
            GPIO_Speed_2MHz,
            GPIO_Speed_50MHz
        } GPIOSpeed_TypeDef;
    GPIOMode_TypeDef  GPIO 工作模式枚举类型
        typedef enum
        {
            GPIO_Mode_AIN = 0x0,           // 模拟输入
            GPIO_Mode_IN_FLOATING = 0x04,  // 浮空输入模式
            GPIO_Mode_IPD = 0x28,          // 下拉输入模式
            GPIO_Mode_IPU = 0x48,          // 上拉输入模式
            GPIO_Mode_Out_OD = 0x14,       // 通用开漏输出模式
            GPIO_Mode_Out_PP = 0x10,       // 通用推挽输出模式
            GPIO_Mode_AF_OD = 0x1C,        // 复用开漏输出模式
            GPIO_Mode_AF_PP = 0x18         // 复用推挽输出模式
        } GPIOMode_TypeDef;
    */
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;        // 对应引脚编号为 2
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz; // GPIO 引脚输出速度为 2 MHz
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;  // 复用推挽输出模式

    /*
    void GPIO_Init(GPIO_TypeDef *GPIOx, GPIO_InitTypeDef *GPIO_InitStruct)
        GPIO 模块对应目标引脚初始化函数，所需参数为
            GPIO_TypeDef *GPIO 对应当前模块宏定义，标准库已定义，例如 GPIOA
            GPIO_InitTypeDef *GPIO_InitStruct 初始化目标引脚对应结构体参数地址
    */
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // 2.2 PA3 USART2 Rx 浮空输入模式
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;             // 对应引脚编号为 3
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING; // 浮空输入模式
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // 3. USART2 必要配置
    /*
    typedef struct
    {
        uint32_t USART_BaudRate;            // 波特率
        uint16_t USART_WordLength;          // 字长，对应 8 位数据位 or 9 位数据位
        uint16_t USART_StopBits;            // 停止位
        uint16_t USART_Parity;              // 是否采用校验位
        uint16_t USART_Mode;                // 当前 USART 开启模式，Tx 和 Rx
        uint16_t USART_HardwareFlowControl; // 硬件控制 or 软件控制
    } USART_InitTypeDef;
    */
    USART_InitTypeDef USART_InitStructure = {0};

    // 3.1 8 N 1 配置
    USART_InitStructure.USART_WordLength = USART_WordLength_8b; // 数据位/字长 8 位
    USART_InitStructure.USART_Parity = USART_Parity_No;         // 不使用校验位
    USART_InitStructure.USART_StopBits = USART_StopBits_1;      // 数据停止位 1 位
    // 3.2 波特率配置
    USART_InitStructure.USART_BaudRate = brr; // 用于提供波特率参数直接赋值给当前 USART_BaudRate
    // 3.3 发送 Tx 和 接收 Rx 使能
    // USART_Mode_Tx 发送模块使能
    // USART_Mode_Rx 接收模块使能
    USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
    // 3.4 补充配置，当前 USART 为软件触发模式
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;

    /*
    void USART_Init(USART_TypeDef* USARTx, USART_InitTypeDef* USART_InitStruct)
        USART 初始化函数，告示指定串口和对应 USART_InitTypeDef 结构体变量
    */
    USART_Init(USART2, &USART_InitStructure);

    // 4. NVIC 中断配置，需要注册中断有
    // RXNE 接收数据缓冲区非空中断，对应数据接收过程
    // IDLE 数据总线空闲中断，对应数据接收完成
    /*
    void USART_ITConfig(USART_TypeDef *USARTx, uint16_t USART_IT, FunctionalState NewState)
        USART 中断使能 or 失能函数
        USART_TypeDef *USARTx 对应配置哪一个串口，当前对应 USART2
        uint16_t USART_IT     当前需要配置的 USART 中断标志
                                USART_IT_IDLE 数据总线空闲中断
                                USART_IT_RXNE 接收数据缓冲区非空中断
        FunctionalState NewState 对应中断标志是否使能
    */
    // 【删除】IDLE 中断使能
    // USART_ITConfig(USART2, USART_IT_IDLE, ENABLE);
    USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);

    NVIC_InitTypeDef NVIC_InitStructure = {0};
    /*
    typedef struct
    {
        // IRQn 中断请求编号
        uint8_t NVIC_IRQChannel;
        // 中断对应【抢占优先级 占先优先级】
        uint8_t NVIC_IRQChannelPreemptionPriority;
        // 中断对应【次级优先级】
        uint8_t NVIC_IRQChannelSubPriority;
        // 当前中断是否开启标志位
        FunctionalState NVIC_IRQChannelCmd;
    } NVIC_InitTypeDef;
    */
    // 对应 USART2 中断请求编号
    NVIC_InitStructure.NVIC_IRQChannel = USART2_IRQn;
    // 优先级内容后续讲解，目前设置抢占优先级为 0，次级为 1
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
    // 开启当前 USART2_IRQn 中断。
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    /*
    初始化当前 NVIC 中断管理控制器，告知当前 USART2 中断请求开启，并且设置对应优先级。
    */
    NVIC_Init(&NVIC_InitStructure);

    // 5. 开启 USART2 模块
    /*
    void USART_Cmd(USART_TypeDef *USARTx, FunctionalState NewState)
        开启 or 关闭指定 USART 模块函数
            USART_TypeDef *USARTx    指定串口模块
            FunctionalState NewState 当前串口状态
    */
    USART_Cmd(USART2, ENABLE);
}

USART2_Data usart2_value = {0};
void USART2_IRQHandler(void)
{
    /*
    1. 数据缓冲区清除
    2. 【删除】IDLE 中断处理
    3. RXNE 中断处理
    */

    if (usart2_value.flag)
    {
        memset(&usart2_value, 0, sizeof(USART2_Data));
    }

#if 0
    // 【删除】IDLE 中断处理
    // 2. IDLE 中断处理
    /*
    ITStatus USART_GetITStatus(USART_TypeDef *USARTx, uint16_t USART_IT)
        获取指定 USART 模块中断标志位状态。
    */
    if (USART_GetITStatus(USART2, USART_IT_IDLE))
    {
        // 【清除中断标志位】完全按照寄存器方式完成
        USART2->SR;
        USART2->DR;

        usart2_value.flag = 1;

        /*
        TODO 临时数据展示处理，通过 USART1 模块发送数据到 PC 串口调试助手
        */
        USART1_SendBuffer(usart2_value.data, usart2_value.count);
    }
#endif
    u16 data = 0;
    // 3. RXNE 中断处理，按照 Modbus 要求进行处理
    if (USART_GetITStatus(USART2, USART_IT_RXNE))
    {
        // 1. 将数据暂存
        data = USART_ReceiveData(USART2); 

        // 2. 保证在收到数据之后，立刻将定时计数器 CNT 重新归零
        TIM_SetCounter(TIM7, 0);

        // 3. 判断当前 usart2_value 数据缓冲区中的 count 是否为 0
        if (!usart2_value.count)
        {
            // 如果为 0 表示第一次收到数据，【启动定时器】
            TIM_Cmd(TIM7, ENABLE);
        }

        // 4. 将数据存储到 USART2 接收数据缓冲区中。
        usart2_value.data[usart2_value.count++] = data;
    }
}

void USART2_SendByte(u8 byte)
{
    /*
    1. 判断上一次数据是否发送完成，需要读取 USART 模块 TC 标志位信息
    FlagStatus USART_GetFlagStatus(USART_TypeDef *USARTx, uint16_t USART_FLAG)
        获取指定串口对应标志位状态
        USART_TypeDef *USARTx 指定 USART 模块
        uint16_t USART_FLAG   期望读取哪一个标志位，例如 USART_FLAG_TC

    typedef enum {RESET = 0, SET = !RESET} FlagStatus, ITStatus;
    */
    while (!USART_GetFlagStatus(USART2, USART_FLAG_TC))
        ;

    /*
    void USART_SendData(USART_TypeDef *USARTx, uint16_t Data)
        指定串口数据发送函数，当前是利用 USART2 完成数据发送。
    */
    USART_SendData(USART2, byte);
}

void USART2_SendBuffer(u8 *buffer, u16 count)
{
    while (count--)
    {
        USART2_SendByte(*buffer);
        buffer += 1;
    }
}

void USART2_SendString(const char *str)
{
    while (*str)
    {
        USART2_SendByte(*str);
        str += 1;
    }
}
