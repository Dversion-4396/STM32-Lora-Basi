#include "usart3.h"

/**
 * @brief USART3 模块初始化函数，要求用户提供对应工作波特�?
 *			Tx ==> PB10 【复用推挽输出模式�?
 *	        Rx ==> PB11	【浮空输入模式�?
 *          NRZ 数据格式�?8N1 模式
 *
 * @param brr 用户提供的波特率参数，需要根据当�?MCU 时钟频率
 *				进行合理选择
 */
void USART3_Init(u32 brr)
{
	GPIO_InitTypeDef  gpio_init_struct;
	USART_InitTypeDef usart_init_struct;
	NVIC_InitTypeDef  nvic_init_struct;

	// 1. RCC 时钟使能 GPIOB 模块（APB2）�?USART3 模块（APB1�?
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3, ENABLE);

	/*
	2. GPIOB 对应 IO 引脚配置
		Tx ==> PB10 【复用推挽输出模式�?
		Rx ==> PB11 【浮空输入模式�?
	*/
	gpio_init_struct.GPIO_Pin   = GPIO_Pin_10;
	gpio_init_struct.GPIO_Mode  = GPIO_Mode_AF_PP;
	gpio_init_struct.GPIO_Speed = GPIO_Speed_10MHz;
	GPIO_Init(GPIOB, &gpio_init_struct);

	gpio_init_struct.GPIO_Pin  = GPIO_Pin_11;
	gpio_init_struct.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(GPIOB, &gpio_init_struct);

	/*
	3. USART3 模块配置
		3.1 8N1 NRZ 数据格式
		3.2 波特率设置，【重点】当�?STM32F103ZET6 USART3 对应时钟分支
			时钟基础频率�?36 MHz（标准库 USART_Init 内部自动完成 BRR 计算�?
		3.3 使能端口 Tx(TE) �?Rx(RE)
	*/
	usart_init_struct.USART_BaudRate            = brr;
	usart_init_struct.USART_WordLength          = USART_WordLength_8b; // 8 位数据位
	usart_init_struct.USART_StopBits            = USART_StopBits_1;    // 1 位停止位
	usart_init_struct.USART_Parity              = USART_Parity_No;     // 无校验位
	usart_init_struct.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	usart_init_struct.USART_Mode                = USART_Mode_Tx | USART_Mode_Rx;
	USART_Init(USART3, &usart_init_struct);

	/*
	4. 中断相关配置
		4.1 设置当前 USART3 打开 RXNE �?IDLE 中断
		4.2 使能 NVIC 注册对应 USART3_IRQn 同时配置优先�?
			【重点】NVIC 优先级分组已经在 USART1_Init 中完成设置，
			要求 USART1_Init() 先于 USART3_Init() 调用
	*/
	USART_ITConfig(USART3, USART_IT_RXNE, ENABLE);
	USART_ITConfig(USART3, USART_IT_IDLE, ENABLE);

	nvic_init_struct.NVIC_IRQChannel                   = USART3_IRQn;
	nvic_init_struct.NVIC_IRQChannelPreemptionPriority = 1;
	nvic_init_struct.NVIC_IRQChannelSubPriority        = 1;
	nvic_init_struct.NVIC_IRQChannelCmd                = ENABLE;
	NVIC_Init(&nvic_init_struct);

	// 5. 使能 USART3 总开�?UE
	USART_Cmd(USART3, ENABLE);
}

/*
注意：全局变量声明需要在头文件中使用 extern 关键字标注，表示当前变量为外部文件定�?
*/
USART3_Data usart3_value = {0};

/*
当前中断处理函数直接实现
	【修正】原寄存器版本中 USART1_SendBuffer(usart3_value.data, usart1_value.count)
		第二个参数误用了 usart1_value.count，当前版本已更正�?usart3_value.count
*/
void USART3_IRQHandler(void)
{
	/*
	[FIX-BUG2] ����ԭ����һ�� "if (usart3_value.flag) memset(&usart3_value, 0, ...)"��
	           ����������һ֡��û����ѭ��ȡ��ʱ���������ݡ�ֻ��������֡��
	           ������һ�����������ã�
	           (a) �������պ����� 256 �ֽ�ʱ��count=256 �� flag=1��
	               �������� IDLE �жϻ���ִ����� memset������֡��գ�
	               Ȼ�� IDLE ��֧�ְ� flag �� 1 �� count=0��
	               ��ѭ��ȡ����֡��rxLen=0������֡���ݶ�ʧ��
	           (b) ��ѭ���� DHT20 �ɼ����� 100~200ms �ڼ���������յ���֡��
	               �ڶ�֡�ĵ�һ���ֽڽ����Ͱѵ�һ֡����ˡ�
	           �������ְ����ȫ���������� USART3_TakeFrame()�����ж��ٽ�����
	           ��ȫ����+��գ����ж���ֻ�������ֽڣ����������建������
	           �������߳��ڲ�ȡ��RXNE ��֧�� count<BUFFER_SIZE �жϻᶪ�����ֽڣ�
	           ͬ������Խ�磬Ҳ�����ƻ����յ���һ֡��
	*/


	/*
	2. RXNE 中断处理
		2.1 判断当前 USART3 是否�?RXNE 中断触发
		2.2 USART_ReceiveData() 读取 DR 寄存器的同时，RXNE 中断标志位自动清�?
		2.3 数据存储�?USART3_Data 结构�?u8 类型数组�?
	*/
	if (USART_GetITStatus(USART3, USART_IT_RXNE) != RESET)
	{
		/*
		���޸�Խ�硿ԭ���������� data[count++] д�룬һ��������������ѭ����û���ü�
			ȡ�����ݣ�DHT20 ���β��������� 100~200ms���ڼ� LoRa ���������յ���֡����
			count �����������Խ�� 256 �ֽ�����߽磬�ƻ������ڴ档
			����ĳɣ���������ʱ�������ֽڣ���֤�������Խ�硣
		*/
		u8 rxByte = (u8)USART_ReceiveData(USART3);

		if (usart3_value.count < USART3_BUFFER_SIZE)
		{
			usart3_value.data[usart3_value.count++] = rxByte;
		}

		if (USART3_BUFFER_SIZE == usart3_value.count)
		{
			// 数据接收缓冲区已满，flag 标志位为 1
			usart3_value.flag = 1;

            /*
			【DMA 改造】此处原有三�?USART1 打印�?USART3 RX : " + 数据 + "\r\n"），
				已移�?main() 主循环中检�?usart3_value.flag 后统一打印�?
				原因：USART1 �?DMA 后，连续三次调用会互相覆�?DMA 配置�?
					导致 PC 端收到残缺内容；同时中断内也不应做长耗时打印
			*/

			///*
			//数据临时处理
			//	USART3 目前连接的是 LoRa 模块
			//	USART1 是连接到 PC 端，可以在串口调试助手看到数�?
			//*/
			//USART1_SendString("USART3 RX : ");
			//USART1_SendBuffer(usart3_value.data, usart3_value.count);
			//USART1_SendString("\r\n");
		}
	}

	/*
	3. IDLE 中断处理
		【重点】IDLE 中断标志位清除方式：先读�?USART3 SR 寄存器，
			再读�?USART3 DR 寄存�?
		USART_GetITStatus() 内部已经读取�?SR 寄存器，
		所以这里只需要再调用 USART_ReceiveData() 读取 DR 寄存器即�?
	*/
	if (USART_GetITStatus(USART3, USART_IT_IDLE) != RESET)
	{
		USART_ReceiveData(USART3);

		// 数据接收完成，flag 标志位为 1
		usart3_value.flag = 1;

        /*
		【DMA 改造】此处原有三�?USART1 打印�?USART3 RX : " + 数据 + "\r\n"），
			已移�?main() 主循环中统一打印，原因同�?
		*/

		///*
		//数据临时处理
		//	USART3 目前连接的是 LoRa 模块
		//	USART1 是连接到 PC 端，可以在串口调试助手看到数�?
		//*/
		//USART1_SendString("USART3 RX : ");
		//USART1_SendBuffer(usart3_value.data, usart3_value.count);
		//USART1_SendString("\r\n");
	}
}

/**
 * @brief USART3 模块发送一个字节数据到接收�?
 *
 * @param byte 需要进行发送的一个字节数�?
 */
void USART3_SendByte(u8 byte)
{
	/*
	【重点】在每一次数据发送之前，需要确保上一帧数据已经完成发送，
	通过 USART3 SR 寄存器的 TC 标志位进行判�?
	*/
	while (USART_GetFlagStatus(USART3, USART_FLAG_TC) == RESET);

	USART_SendData(USART3, byte);
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
 * @param str 发送的字符串数据内�?
 */
void USART3_SendString(const char *str)
{
	while (*str)
	{
		USART3_SendByte(*str);
		str += 1;
	}
}
