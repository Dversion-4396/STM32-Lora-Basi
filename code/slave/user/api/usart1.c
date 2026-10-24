#include "usart1.h"

/*
【DMA 改造说明�?
	发送方向由 CPU 逐字节搬运改�?DMA1_Channel4 自动搬运�?
	接收方向保持不变（仍�?RXNE + IDLE 中断）�?
	对外接口（SendByte / SendBuffer / SendString）的名字和参数完全不变，
	所以调用点不需要改函数名，只需要把“多段连续打印”合并成“一次打印”�?
*/

/* DMA 发送缓冲区，必须是静态变量，DMA 搬运期间不能被释放或改写 */
static u8 usart1_tx_buffer[USART1_TX_BUFFER_SIZE];

/* 发送忙标志�? 表示 DMA 正在搬运数据�? 表示空闲 */
static volatile u8 usart1_tx_busy = 0;


/**
 * @brief USART1 模块初始化函数，需要用户提供对应的波特�?
 *			Tx ==> PA9  【复用推挽输出模式�?
 *	        Rx ==> PA10	【浮空输入模式�?
 *          NRZ 数据格式�?8N1 模式
 *
 * @param brr 用户提供的波特率参数，需要根据当�?MCU 时钟频率
 *				进行合理选择
 */
void USART1_Init(u32 brr)
{

    // typedef struct
    // {
    //   uint16_t GPIO_Pin;            //引脚编号
    //   GPIOSpeed_TypeDef GPIO_Speed; //引脚速率
    //   GPIOMode_TypeDef GPIO_Mode;   //引脚模式
    // }GPIO_InitTypeDef;
	GPIO_InitTypeDef  gpio_init_struct;

    // 	typedef struct
    // {
    //   uint32_t USART_BaudRate;            //波特�? 
    //   uint16_t USART_WordLength;          //数据字节长度  
    //   uint16_t USART_StopBits;            //停止�?
    //   uint16_t USART_Parity;              //校验�? 
    //   uint16_t USART_Mode;                //模式
    //   uint16_t USART_HardwareFlowControl; //流控制（防止死磕数据�?
    // } USART_InitTypeDef;
	USART_InitTypeDef usart_init_struct;

    // 	typedef struct
    // {
    //   uint8_t NVIC_IRQChannel;                   //中断编号
    //   uint8_t NVIC_IRQChannelPreemptionPriority; //优先�?
    //   uint8_t NVIC_IRQChannelSubPriority;        //子优先级
    //   FunctionalState NVIC_IRQChannelCmd;        //控制中断请求
    // } NVIC_InitTypeDef;
	NVIC_InitTypeDef  nvic_init_struct;

	DMA_InitTypeDef   dma_init_struct;

	// 1. RCC 时钟使能 GPIOA 模块�?USART1 模块（APB2�?,DMA1 模块（AHB�?
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_USART1, ENABLE);

    // 【DMA 改造】DMA1 挂在 AHB 总线上，必须单独使能时钟，否则后�?DMA 配置无效
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);

	/*
	2. GPIOA 对应 IO 引脚配置
		Tx ==> PA9  【复用推挽输出模式�?
		Rx ==> PA10	【浮空输入模式�?
	*/
	gpio_init_struct.GPIO_Pin   = GPIO_Pin_9;
	gpio_init_struct.GPIO_Mode  = GPIO_Mode_AF_PP;
	gpio_init_struct.GPIO_Speed = GPIO_Speed_10MHz;
	GPIO_Init(GPIOA, &gpio_init_struct);

	gpio_init_struct.GPIO_Pin  = GPIO_Pin_10;
	gpio_init_struct.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(GPIOA, &gpio_init_struct);

	/*
	3. USART1 模块配置
		3.1 8N1 NRZ 数据格式
		3.2 波特率设置，【重点】当�?STM32F103ZET6 USART1 对应时钟分支
			时钟基础频率�?72 MHz（标准库 USART_Init 内部自动完成 BRR 计算�?
		3.3 使能端口 Tx(TE) �?Rx(RE)
	*/
	usart_init_struct.USART_BaudRate            = brr;//brr是传入的参数，即串口波特�?
	usart_init_struct.USART_WordLength          = USART_WordLength_8b; // 8 位数据位
	usart_init_struct.USART_StopBits            = USART_StopBits_1;    // 1 位停止位
	usart_init_struct.USART_Parity              = USART_Parity_No;     // 无校验位
	usart_init_struct.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	usart_init_struct.USART_Mode                = USART_Mode_Tx | USART_Mode_Rx;
	USART_Init(USART1, &usart_init_struct);

	/*
	4. 中断相关配置
		4.1 设置当前 USART1 打开 RXNE �?IDLE 中断
		4.2 使能 NVIC 注册对应 USART1_IRQn 同时配置优先�?
	*/
	USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);
	USART_ITConfig(USART1, USART_IT_IDLE, ENABLE);

	// 【重点】NVIC 优先级分组，整个工程只需要设置一次，统一放在 USART1_Init �?
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);

	nvic_init_struct.NVIC_IRQChannel                   = USART1_IRQn;
	nvic_init_struct.NVIC_IRQChannelPreemptionPriority = 1;
	nvic_init_struct.NVIC_IRQChannelSubPriority        = 0;
    // 	typedef enum 
    // {
    //   DISABLE = 0, 
    //   ENABLE = 1 
    // } FunctionalState;
	nvic_init_struct.NVIC_IRQChannelCmd                = ENABLE;
	NVIC_Init(&nvic_init_struct);

    /*
	5. 【DMA 改造】USART1 发�?DMA 通道配置
		5.1 STM32F103 �?USART1_TX 固定对应 DMA1_Channel4，通道不能自�?
		5.2 源地址固定�?usart1_tx_buffer，每次发送只改传输长度；
			地址永远不变，可以避免“只改长度不改地址”导致永远发第一帧的错误
		5.3 单次模式（DMA_Mode_Normal），每帧发送前重新启动一�?
	*/
	DMA_DeInit(DMA1_Channel4);

	dma_init_struct.DMA_PeripheralBaseAddr = (u32)&USART1->DR;				// 目的地：USART1 数据寄存�?
	dma_init_struct.DMA_MemoryBaseAddr     = (u32)usart1_tx_buffer;			// 源：DMA 发送缓冲区
	dma_init_struct.DMA_DIR                = DMA_DIR_PeripheralDST;			// 传输方向：内�?==> 外设
	dma_init_struct.DMA_BufferSize         = 1;						// [FIX-BUG5] ��ֵ���� >=1����Լ�� 1..65535����ÿ�η���ʱ�ٸ�
	dma_init_struct.DMA_PeripheralInc      = DMA_PeripheralInc_Disable;		// 外设地址固定，不自增
	dma_init_struct.DMA_MemoryInc          = DMA_MemoryInc_Enable;			// 内存地址自增，逐字节搬�?
	dma_init_struct.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;	// 外设数据宽度 8 �?
	dma_init_struct.DMA_MemoryDataSize     = DMA_MemoryDataSize_Byte;		// 内存数据宽度 8 �?
	dma_init_struct.DMA_Mode               = DMA_Mode_Normal;				// 单次模式，不用循环模�?
	dma_init_struct.DMA_Priority           = DMA_Priority_Medium;			// 优先级中等即�?
	dma_init_struct.DMA_M2M                = DMA_M2M_Disable;				// 关闭内存到内存模�?
	DMA_Init(DMA1_Channel4, &dma_init_struct);

	DMA_Cmd(DMA1_Channel4, DISABLE);

	/*
	【重点】清掉上�?复位后可能残留的传输完成标志�?
		否则 USART1_WaitTxComplete() 第一次调用会误判“已完成”而立即返回，
		结果第一帧数据还没发出去就被下一帧覆�?
	*/
	DMA_ClearFlag(DMA1_FLAG_TC4 | DMA1_FLAG_GL4);

	// 6. 【DMA 改造】允�?USART1 的发送请求连接到 DMA1_Channel4
	USART_DMACmd(USART1, USART_DMAReq_Tx, ENABLE);


	// 7. 使能 USART1 总开�?UE
	USART_Cmd(USART1, ENABLE);
}

/*
注意：全局变量声明需要在头文件中使用 extern 关键字标注，表示当前变量为外部文件定�?
*/
USART1_Data usart1_value = {0};

/*
当前中断处理函数直接实现
*/
void USART1_IRQHandler(void)
{
	/*
	[FIX-BUG2] ����ԭ����һ�� "if (usart1_value.flag) memset(&usart1_value, 0, ...)"��
	           ���ڻ������պ����� 256 �ֽ�ʱ������� IDLE �ж������֡��
	           Ҳ������ѭ���� DHT20 �����ڼ��ǰһ֡�����
	           �������ְ�𽻸������� USART1_TakeFrame()�����ж��ٽ�����
	           ��ȫ����+��գ����ж�ֻ�������ֽڡ�
	*/


	/*
	2. RXNE 中断处理（每个收到一个字节触发一次）
		2.1 判断当前 USART1 是否�?RXNE 中断触发
		2.2 USART_ReceiveData() 读取 DR 寄存器的同时，RXNE 中断标志位自动清�?
		2.3 数据存储�?USART1_Data 结构�?u8 类型数组�?
	*/
	if (USART_GetITStatus(USART1, USART_IT_RXNE) != RESET)
	{
		/*
		���޸�Խ�硿ԭ���������� data[count++] д�룬���������� count ��Խ�� 256 �ֽ�
			����߽��ƻ������ڴ档��Ϊ��������������Խ�硣
		*/
		u8 rxByte = (u8)USART_ReceiveData(USART1);

		if (usart1_value.count < USART1_BUFFER_SIZE)
		{
			usart1_value.data[usart1_value.count++] = rxByte;
		}

		if (USART1_BUFFER_SIZE == usart1_value.count)
		{
			// 数据接收缓冲区已满，flag 标志位为 1
			usart1_value.flag = 1;
            /*
			【DMA 改造】此处原有三�?USART1 打印�?RXNE : " + 数据 + "\r\n"），
				已移到主循环检�?flag 后统一打印�?
				原因：中断内启动 DMA 会与主循环的发送互相覆盖，
					且阻塞发送会让中断占用时间过�?
			*/
			//// 将数据通过 USART1 发送到当前 PC 端调试工�?
			//USART1_SendString("RXNE : ");
			//USART1_SendBuffer(usart1_value.data, usart1_value.count);
			//USART1_SendString("\r\n");
		}
	}

	/*
	3. IDLE 中断处理（数据传输结束，触发�?
		【重点】IDLE 中断标志位清除方式：先读�?USART1 SR 寄存器，
			再读�?USART1 DR 寄存�?
		USART_GetITStatus() 内部已经读取�?SR 寄存器，
		所以这里只需要再调用 USART_ReceiveData() 读取 DR 寄存器即�?
	*/
	if (USART_GetITStatus(USART1, USART_IT_IDLE) != RESET)  //USART_GetITStatus() 函数已经读了 SR 寄存�?
	{
		USART_ReceiveData(USART1); //读取并取出数据，此处只是为了清除 DR 寄存器，返回值丢�?

		// 数据接收完成，flag 标志位为 1
		usart1_value.flag = 1;
        
        /*
		【DMA 改造】此处原有三�?USART1 打印�?IDLE : " + 数据 + "\r\n"），
			已移到主循环检�?flag 后统一打印，原因同�?
		*/

		//// 临时将数据打印到 PC 端调试工�?
		//USART1_SendString("IDLE : ");
		//USART1_SendBuffer(usart1_value.data, usart1_value.count);
		//USART1_SendString("\r\n");
	}
}

/**
 * @brief USART1 模块发送一个字节数据到接收�?
 *
 * @param byte 需要进行发送的一个字节数�?
 */
void USART1_SendByte(u8 byte)
{

    /*
	【DMA 改造】逐字节发送前先等 DMA 空闲�?
		否则会和 DMA 同时�?DR 寄存器，导致数据错乱
	*/
	USART1_WaitTxComplete();

	/*
	【重点】在每一次数据发送之前，需要确保上一帧数据已经完成发送，
	通过 USART1 SR 寄存器的 TC 标志位进行判�?
	*/
	while (USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET)
	{
	}

	/*
	数据发送到数据寄存�?DR 中，�?USART 数据发送控制器
	将数据按照当�?USART 配置发送到接收�?
	*/
	USART_SendData(USART1, byte);
}

///**
// * @brief USART1 模块指定字节个数数据内容到接收端
// *
// * @param buffer 发送数据对应内存空间首地址
// * @param len    发送目标数据字节数
// */
//void USART1_SendBuffer(u8 *buffer, u16 len)
//{
//	while (len--)
//	{
//		USART1_SendByte(*buffer);
//		buffer += 1;
//	}
//}

/**
 * @brief USART1 模块指定字节个数数据内容到接收端（DMA 方式�?
 *			【重点】函数返回时数据已经复制�?DMA 缓冲区并启动传输�?
 *				但可能尚未发送到线上；需要确保发完请调用 USART1_WaitTxComplete()
 *
 * @param buffer 发送数据对应内存空间首地址，可以是局部数�?
 * @param len    发送目标数据字节数，超�?USART1_TX_BUFFER_SIZE 会被截断
 */
void USART1_SendBuffer(u8 *buffer, u16 len)
{
	if ((buffer == 0) || (len == 0))
	{
		return;
	}

	// 超出缓冲区容量部分直接丢弃，防止数组越界写坏静态缓冲区
	if (len > USART1_TX_BUFFER_SIZE)
	{
		len = USART1_TX_BUFFER_SIZE;
	}

	/*
	【重点】只有一个发送缓冲区，上一帧没发完时新数据会覆盖正在搬运的内容�?
		所以必须先等上一帧完成�?
		复制一份数据的目的：调用者可以放心使用局部数组，
		函数返回后原缓冲区被改写也不会影响发送内�?
	*/
	USART1_WaitTxComplete();
	memcpy(usart1_tx_buffer, buffer, len);

	/*
	【重点】每次发送必须重新设置传输长度并重新启动 DMA�?
		单次模式�?DMA 传完就停，不会自动重�?
	*/
	DMA_Cmd(DMA1_Channel4, DISABLE);
	DMA_SetCurrDataCounter(DMA1_Channel4, len);
	DMA_ClearFlag(DMA1_FLAG_TC4);
	DMA_Cmd(DMA1_Channel4, ENABLE);

	usart1_tx_busy = 1;
}


///**
// * @brief USART1 模块发送字符串数据到接收端
// *
// * @param str 发送的字符串数据内�?
// */
//void USART1_SendString(const char *str)
//{
//	while (*str)
//	{
//		USART1_SendByte(*str);
//		str += 1;
//	}
//}

/**
 * @brief USART1 模块发送字符串数据到接收端（DMA 方式�?
 *			【重点】内部先统计字符串长度再调用 USART1_SendBuffer()�?
 *				不要逐字节调�?USART1_SendByte()，否则每字节都要等一�?DMA 空闲
 *
 * @param str 发送的字符串数据内容，必须�?'\0' 结尾
 */
void USART1_SendString(const char *str)
{
	u16 len = 0;

	if (str == 0)
	{
		return;
	}

	while (str[len] != '\0')
	{
		len += 1;
	}

	USART1_SendBuffer((u8 *)str, len);
}

/**
 * @brief 等待当前 DMA 发送帧真正完成（数据全部移到串口线上）
 *			【重点】先�?DMA 传输完成，再�?USART �?TC 标志�?
 *				两者相差约 1 个字节时间（9600bps 下约 1.04ms�?
 */
void USART1_WaitTxComplete(void)
{
	if (usart1_tx_busy == 0)
	{
		return;
	}

	// 1. 等待 DMA 把缓冲区内所有字节搬运到 DR 寄存�?
	while (DMA_GetFlagStatus(DMA1_FLAG_TC4) == RESET);

	// 2. 等待最后一个字节真正从移位寄存器发送到线上
	while (USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET);

	DMA_ClearFlag(DMA1_FLAG_TC4);	// 清除标志，否则下一帧会误判已完�?
	usart1_tx_busy = 0;
}
