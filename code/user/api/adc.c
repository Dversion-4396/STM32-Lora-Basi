#include "adc.h"

/*
【DMA 补充】
    将 ADC 模块中对应的 DR 数据直接利用 DMA 搬运到 lsen_value 变量中。
    【片上外设数据】==> 【MCU 内存】

【整体开发流程】
    1. DMA 模块时钟使能，当前搬运数据对应的片上外设模块对应 ADC3
        根据 DMA 模块通道片上外设映射关系，所需的 DMA 通道为
            DMA2_Channel5
    【需要时钟使能 DMA 模块】所在时钟分析为 AHB
    2. DMA 模块配置初始化
    3. DMA 触发方式配置
    4. 【????????】
*/
u16 lsen_value = 0;

/**
 * @brief ADC3 模块初始化，需要完成
 *      1. 时钟使能 ADC3 模块和 GPIOF 模块
 *      2. PF8 配置为【模拟输入模式】
 *      3. ADC 相关配置
 */
void ADC3_Init(void)
{
    // 1. 时钟使能 GPIOF 和 ADC3
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC3 | RCC_APB2Periph_GPIOF, ENABLE);

    // 2. PF8 配置为【模拟输入模式】
    GPIO_InitTypeDef GPIO_InitStructrue = {0};

    GPIO_InitStructrue.GPIO_Pin = GPIO_Pin_8;
    GPIO_InitStructrue.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOF, &GPIO_InitStructrue);

    /*
    3. ADC 模块配置
    */
    /*
    3.1 ADC 时钟预分频倍数配置，要求提供给 ADC 模块的时钟最大为 14 MHz
        72 / 6 ==> 12 MHz < 14 MHz 满足要求
    */
    RCC_ADCCLKConfig(RCC_PCLK2_Div6);

    /*
    3.2 ADC 模块基本配置
    typedef struct
    {
        uint32_t ADC_Mode;
        FunctionalState ADC_ScanConvMode;
        FunctionalState ADC_ContinuousConvMode;
        uint32_t ADC_ExternalTrigConv;
        uint32_t ADC_DataAlign;
        uint8_t ADC_NbrOfChannel;
    } ADC_InitTypeDef;
    */
    ADC_InitTypeDef ADC_InitStructure = {0};

    // 当前使用 ADC3 模块，采用独立模式，不需要考虑双模式 ADC
    ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;
    // 因为当前仅需要使用 ADC3_IN6 通道，对应【单通道模式 DISABLE】
    ADC_InitStructure.ADC_ScanConvMode = DISABLE;
    // 配置当前 ADC 转换为连续转换或者单次转换。【ENABLE 表示连续 DISABLE 表示单次】
    ADC_InitStructure.ADC_ContinuousConvMode = ENABLE;
    // 当前 ADC 模块转换由【软件触发 ADC_ExternalTrigConv_None】
    ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;
    // ADC 转换数据方式结果对齐方式为【右对齐】
    ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
    // NbrOfChannel ==> Number Of Channel 通道数量控制，范围为 1 ~ 16
    // ADC 转换多路通道开关数量
#if 1
    ADC_InitStructure.ADC_NbrOfChannel = 1;
#else
    // 开发 ADC 多路转换通道 3 个
    ADC_InitStructure.ADC_NbrOfChannel = 3;
#endif
    ADC_Init(ADC3, &ADC_InitStructure);

    /*
    以上操作仅完成 ADC 模块基本初始化，尚未明确使用的 ADC 通道对应关系，目前已知 ADC3
    开放一个多路转换通道。需要明确当前多路转换通道映射关系。

    4. 采用规则转换通道方式转换 ADC 输入 Channel
    void ADC_RegularChannelConfig(ADC_TypeDef *ADCx, uint8_t ADC_Channel, uint8_t Rank, uint8_t ADC_SampleTime)
        ADC 规则通道配置函数，需要提供的参数
        ADC_TypeDef *ADCx
            对应 ADC 模块
        uint8_t ADC_Channel
            指定当前加入到 ADC 规则转换通道的 ADC Channel，，例如 ADC_Channel_6 ADC_Channel_7
        uint8_t Rank
            当前 ADC 输入通道，在多路开发通道的排名，从 1 到 16
        uint8_t ADC_SampleTime
            当前 ADC 通道转换时间，ADC_SampleTime_239Cycles5 239.5 周期
    */
    ADC_RegularChannelConfig(ADC3, ADC_Channel_6, 1, ADC_SampleTime_239Cycles5);
#if 0   
    // 其他通道指定 ADC 输入通道映射
    ADC_RegularChannelConfig(ADC3, ADC_Channel_2, 2, ADC_SampleTime_239Cycles5);
    ADC_RegularChannelConfig(ADC3, ADC_Channel_10, 3, ADC_SampleTime_239Cycles5);
#endif

    /*
    【DMA】代码实现
    */
    // 【DMA 模块】1. DMA2 AHB 时钟使能
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA2, ENABLE);

    // 【DMA 模块】2. DMA 初始化
    DMA_InitTypeDef DMA_InitStructure = {0};
   
    /*
    片上外设数据寄存器地址，对应 ADC3 模块 DR 寄存
    因为当前 DMA 初始化结构体成员变量 DMA_PeripheralBaseAddr 对应数据类型
    uint32_t，同时内存地址本身就是一个 int 类型数据。
    */
    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)(&(ADC3->DR));
    // 内存变量地址，强转逻辑同上
    DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t)(&lsen_value);

    // 片上外设对应数据搬运到内存中，所需方向 ==> 外设地址为数据源
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralSRC;

    /*
    配置数据宽度
    【注意】DMA_PeripheralDataSize 和 DMA_MemoryDataSize 对应的参数不同，不可以混用。
    当前 ADC 数据和目标变量都是 u16 类型，采用 HalfWord 半字处理
    */
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;

    /*
    配置当前搬运对应的数据源和目标地址是否需要执行增量操作
    【注意】DMA_PeripheralInc 和 DMA_MemoryInc 对应参数不同，需要独立配置。
    */
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Disable;

    // 当前数据方向为片上外设到内存，非内存到内存方式
    DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;

    //【???】选择正常缓冲方式
    //【???】选择循环缓冲方式
    DMA_InitStructure.DMA_Mode = DMA_Mode_Circular;
    // DMA 优先级
    DMA_InitStructure.DMA_Priority = DMA_Priority_VeryHigh;

    // DMA 数据缓冲区字节数
    DMA_InitStructure.DMA_BufferSize = 8;

    /*
    DMA 模块初始化和启动
    */
    DMA_Init(DMA2_Channel5, &DMA_InitStructure);
    DMA_Cmd(DMA2_Channel5, ENABLE);

    /*
    任何一个片上外设模块需要开启 DMA 模式，无论是作为数据源还是目标位置
    都需要当前片上外设模块运行 DMA 模式
    */
    ADC_DMACmd(ADC3, ENABLE);

    /*
    5. ADC 校准流程！！！
    */
    /*
    5.1 启动 ADC
        如果未开启 ADC 模块，无法进入校准流程，需要给 ADC 通电
    */
    ADC_Cmd(ADC3, ENABLE);

    /*
    5.2 复位校准
        实际是是在 ADCx->CR2 寄存器对应 RSTCAL 赋值 1
        如果硬件校准完毕，对应 RSTCAL 为 0
    */
    ADC_ResetCalibration(ADC3);
    while (ADC_GetResetCalibrationStatus(ADC3))
        ;
    /*
    ADC3->CR2 |= (0x01 << 3);
    while (ADC3->CR2 & (0x01 << 3));
    */

    /*
    5.3 A/D 校准
        实际是是在 ADCx->CR2 寄存器对应 CAL 赋值 1
        如果硬件校准完毕，对应 CAL 为 0
    */
    ADC_StartCalibration(ADC3);
    while (ADC_GetCalibrationStatus(ADC3))
        ;
    /*
    ADC3->CR2 |= (0x01 << 2);
    while (ADC3->CR2 & (0x01 << 2));
    */

    /*
    6. 【当前 ADC 明确软件触发】
    */
    ADC_SoftwareStartConvCmd(ADC3, ENABLE);
}

u16 Get_Lsen_Value(void)
{
    /*
    因为当前 ADC 数据转换完成之后，ADC3->DR 寄存器中的数据
    会自动触发 DMA 搬运到 lsen_value 变量中，直接要求读取
    lsen_value 数据即可。
    */
    return lsen_value;
}