#include "usart1.h"

/**
 * @brief USART1 模块初始化函数，要求用户提供对应工作波特率
 *			Tx ==> PA9  【复用推挽输出模式】
 *	        Rx ==> PA10	【浮空输入模式】
 *          NRZ 数据格式为 8N1 模式
 *
 * @param brr 用户提供的波特率参数，需要根据当前 MCU 时钟频率和
 *				进行合理选择
 */
void USART1_Init(u32 brr)
{
	// 1. RCC 时钟使能 GPIOA 模块和 USART1 模块
	RCC->APB2ENR |= RCC_APB2_GPIOA_CLOCK_ENABLE 
					| RCC_APB2_USART1_CLOCK_ENABLE;
	
	// 2. 对 GPIO 模块进行配置
	/*
	Tx ==> PA9  【复用推挽输出模式】
	Rx ==> PA10	【浮空输入模式】
	*/
	GPIOA->CRH &= ~(0x0F << 8);
	GPIOA->CRH |= (Floating_Input_Mode << 8);
	
	GPIOA->CRH &= ~(0x0F << 4);
	GPIOA->CRH |= (Alternate_Push_And_Pull_Speed_10MHz << 4);
	
	/*
	[可以简单优化一下]
	GPIOA->CRH &= ~(0xFF << 4);
	GPIOA->CRH |= (Floating_Input_Mode << 8) 
			| (Alternate_Push_And_Pull_Speed_10MHz << 4);
	*/
	
	/*
	3. USART 模块配置
		3.1 8N1 NRZ 数据格式
		3.2 USART1 波特率配置
		3.3 使能开放 Tx(TE) 和 Rx(RE)
		3.4 使能 USART1 工作，配置 UE
	*/
	// 【重点操作】明确 USART1 关闭
	USART1->CR1 &= ~(0x01 << 13);
	
	// 3.1 8N1 NRZ 数据格式
	// 配置 CR1 【位 12】 M 对应为 0，数据位为 8 位
	USART1->CR1 &= ~(0x01 << 12);
	// 配置 CR1 【位 10】 PCE 对应为 0，当前数据发送不使用校验位
	USART1->CR1 &= ~(0x01 << 10);
	// 配置 CR2 【位 13:12】 STOP 对应为 [00]，当前数据停止位为 1 位
	USART1->CR2 &= ~(0x03 << 12);
	
	// 3.2 USART1 波特率配置
	// 1. 根据公式计算对应的 usart_div 数据
	float usart_div = 72000000 / 16 / brr;
	// 2. 拆解波特率寄存器所需整数位数据
	u16 usart_mantissa = (u16)usart_div;
	// 3. 拆解波特率寄存器所需小数位数据
	u16 usart_fraction = (u16)((usart_div - usart_mantissa) * 16);
	// 4. 填充 USART BRR 波特率控制寄存器
	USART1->BRR = usart_mantissa << 4 | usart_fraction;
	
	/*
	【补充中断相关配置】
		1. CR1 配置当前 USART1 开启 RXNE 和 IDLE 中断
		2. 使用 NVIC 注册对应 USART1_IRQn 同时配置优先级
			【核心参数】对应当前模块的中断请求编号 USART1_IRQn
	*/
	//【中断配置补充】1. CR1 配置当前 USART1 开启 RXNE 和 IDLE 中断
	USART1->CR1 |= (0x01 << 5); // 使能 RXNE 中断
	USART1->CR1 |= (0x01 << 4); // 使能 IDLE 中断 
	
	//【中断配置补充】2. 使用 NVIC 注册对应 USART1_IRQn 同时配置优先级
	// void NVIC_SetPriority(IRQn_Type IRQn, uint32_t priority) 设置中断优先级
	NVIC_SetPriority(USART1_IRQn, 1);
	// void NVIC_EnableIRQ(IRQn_Type IRQn) 使能当前中断
	NVIC_EnableIRQ(USART1_IRQn);

	// 3.3 使能开放 Tx(TE) 和 Rx(RE)
	USART1->CR1 |= (0x01 << 3); // TE
	USART1->CR1 |= (0x01 << 2); // RE
	
	// 3.4 使能 USART1 工作，配置 UE
	USART1->CR1 |= (0x01 << 13);
}

/*
声明全局变量，并且在头文件中使用 extern 关键字标注，当前变量外部文件可用
*/
USART1_Data usart1_value = {0};

/*
当前函数无需声明，直接实现。
*/
void USART1_IRQHandler(void)
{
	/*
	1. 判断数据接收缓冲区底层 flag 变量数据是否为 1，如果为 1
	表示当前数据是上一次接收完成 或者 处理之后的剩余数据，需要进行
	清除操作。
	*/
	if (usart1_value.flag)
	{
		memset(&usart1_value, 0, sizeof(USART1_Data));
	}
	
	/*
	【处理 RXNE 中断】
		1. 根据 USART1->SR 状态寄存器判断是否为 RXNE 中断触发 
		2. 【重点，清除中断标志位】
			软件清除根据文档要求，是对 USART1->DR 读取操作即可清除
		3. 【数据存储到临时缓冲区】
			需要将 USART1->DR 寄存器中的数据，存储到 USART1_Data 底层
			u8 类型数组中。
	【特殊情况】
		当前 USART1_Data 底层数组有效元素个数已满 == USART1_BUFFER_SIZE
		需要对当前已接收数据进行处理，才可以进行后续的数据接收。
	*/
	if (USART1->SR & (0x01 << 5))
	{
		/*
		1. 数据需要存储到 USART1_Data 底层 u8 类型 data 数组中
		2. usart1_value.count++ 作为下标操作，先提供数据，在累加数值
			可以满足数组【尾插法】方式数据添加
		3. USART1->DR 对当前 DR 进行读取操作，提取数据，同时【清除中断】
		*/
		usart1_value.data[usart1_value.count++] = USART1->DR;
		
		if (USART1_BUFFER_SIZE == usart1_value.count)
		{
			// 数据已满，flag 标志位为 1
			usart1_value.flag = 1;
			
			// 当前 USART1_Data 底层数据已满，需要对数据进行处理	
			// 【TODO】当前操作是临时数据，非后续唯一处理方式。
			
			// 将数据通过 USART1 发送到当前 PC 串口调试工具
			USART1_SendString("RXNE : ");
			USART1_SendBuffer(usart1_value.data, usart1_value.count);
			USART1_SendString("\r\n");
		}
	}
	
	/*
	【处理 IDLE 中断】
		1. 根据 USART1->SR 状态寄存器判断是否为 IDLE 中断触发 
		2. 【重点，清除中断标志位】
			通过软件序列进行中断标志位清除
			先读取 USART1->SR，再读取 USART1->DR 
		3. 【数据处理】
			IDLE 表示数据发送完成，进行数据的必要处理
	*/
	if (USART1->SR & (0x01 << 4))
	{
		/*
		【清除中断标志位】
		先读取 USART1->SR，再读取 USART1->DR 
		在 if 判断中 USART1->SR 已读取，仅需要再次读取 USART1->DR 即可
		*/
		USART1->DR;
		
		// 数据接收完成，flag 标志位为 1
		usart1_value.flag = 1;
		
		// 临时数据处理，数据回显到 PC 串口调试助手
		USART1_SendString("IDLE : ");
		USART1_SendBuffer(usart1_value.data, usart1_value.count);
		USART1_SendString("\r\n");
	}
}

/**
 * @brief USART1 模块发送一个字节数据到接收端
 *
 * @param byte 需要进行发送的一个字节数据
 */
void USART1_SendByte(u8 byte)
{
	/*
	【重点】在每一次数据发送之前，需要明确上一次数据已经完成发送，才可以
	将数据写入到 DR 寄存器中，执行数据发送操作。
	通过 USARTx SR 寄存器中 TC 标志位进行判断
		TC 为 0 表示数据尚未发送完成
		TC 为 1 表示数据发送已完成
	为了方便后续对 TC 数据进行判断，在出现 TC 标志位为 1 的情况下，需要进行
	清除操作，操作方式
		1. 读操作 USARTx SR 寄存器
		2. 写操作 USARTx DR 寄存器
	
	【读操作 USARTx SR 寄存器】实际上是在 while 循环中完成，每一次进行判断
	都需要 USART1->SR 提供目前的数据内容，此操作就是【读取 USARTx SR 寄存器】
	*/
	while (!((0x01 << 6) & USART1->SR));
	
	/*
	数据发送时将数据写入到 USART1 DR 寄存器中，
	数据会转换到 TDR 寄存器中，同时自动搬运到发送移位寄存器。
	之后通过 USART 数据发送控制器将数据按照当前 USART 数据
	要求进行发送
	【当前数据发送就是对 USARTx DR 寄存器 进行写操作】
	*/
	USART1->DR = byte;
}

/**
 * @brief USART1 模块指定字节个数数据内容到接收端
 *
 * @param buffer 发送数据对应内存空间首地址
 * @param len    发送目标数据字节数
 */
void USART1_SendBuffer(u8 *buffer, u16 len)
{
	while (len--)
	{
		USART1_SendByte(*buffer);
		buffer += 1;
	}
}

/**
 * @brief USART1 模块发送字符串数据到接收端
 *
 * @param str 发送的字符串数据内容
 */
void USART1_SendString(const char *str)
{
	while (*str)
	{
		USART1_SendByte(*str);
		str += 1;
	}
}
