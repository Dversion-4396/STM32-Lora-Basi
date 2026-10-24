#include "lora_as32.h"
#include "delay.h"

/*
MD0、MD1、AUX 引脚宏定义，统一分配�?GPIOB 端口�?
�?USART3 引脚（PB10、PB11）共用同一组时�?
*/
#define LORA_GPIO_PORT GPIOB
#define LORA_MD0_PIN   GPIO_Pin_12  //开漏输出，选择低位
#define LORA_MD1_PIN   GPIO_Pin_13  //开漏输出，选择低高
#define LORA_AUX_PIN   GPIO_Pin_14  //浮空输入

/*
【时序说明】详见规格书 AUX 引脚功能
	工作状态切换前应当检�?AUX 引脚状态：
		AUX 低电�?           -> 模块繁忙
		AUX 高电平并保持�?2ms  -> 模块空闲，可以切换工作状�?
		MD0/MD1 跳变�?AUX 继续输出高电平约 3ms 才开始切�?
		AUX 拉低                -> 正在切换状�?
	    AUX 回高并保持约 2ms    -> 状态切换完�?
	状态切换完成后，AUX 输出高电平并保持�?2ms
*/
#define LORA_AUX_WAIT_TIMEOUT  (500)  // 等待 AUX 高电平超时时间，单位 ms
#define LORA_RESP_WAIT_TIMEOUT (1000) // 等待模块应答超时时间，单�?ms
/*
 * [FIX-DBG3] ��������ǰ�ȴ�ģ����У�AUX �ߵ�ƽ���ĳ�ʱʱ�䣬��λ ms��
 *   AS32 ����һ֡���������� 2.4k��70 �ֽ�Լ 230ms���� AUX �Żظߣ�
 *   ���������ģ���ڲ��� FIFO������д�뼴�ɷ��أ�����Ҫ������������ʱ�䡣
 *   ����ȡ 50ms���㹻ģ�鴦�����ֲ����� AUX δ����ʱ��ʱ��������ѭ����
 *   ע�⣺�� LORA_AUX_WAIT_TIMEOUT(500ms) ��ͬ���Ǹ�����ģʽ�л��ȹؼ�ʱ��
 */
#define LORA_AUX_SEND_TIMEOUT  (50)

/**
 * @brief 等待 AUX 引脚输出高电平（模块空闲�?
 *
 * @param timeout 等待超时时间，单�?ms
 * @return 1: 等待成功  0: 等待超时
 */
/*
 * [FIX-BUG4] usart3_value.data �� u8[256]������֤�� 0 ��β��
 *            ֱ�� strstr() ��һֱ�����ң���Ӧ��ǡ������ 256 �ֽھ�Խ�����
 *            �����ô��������޵İ�ȫ���ң�����Խ�� count �ֽڡ�
 * @return 1 = �� buf[0..len-1] ���ҵ� needle  0 = δ�ҵ�
 */
static u8 LORA_MemFind(const u8 *buf, u16 len, const char *needle)
{
    u16 nlen = 0;
    u16 i;

    if ((buf == 0) || (needle == 0))
    {
        return 0;
    }

    while (needle[nlen] != 0)
    {
        nlen++;
    }

    if ((nlen == 0) || (len < nlen))
    {
        return 0;
    }

    for (i = 0; i <= (u16)(len - nlen); i++)
    {
        u16 k = 0;
        while ((k < nlen) && (buf[i + k] == (u8)needle[k]))
        {
            k++;
        }
        if (k == nlen)
        {
            return 1;
        }
    }

    return 0;
}

static u8 LORA_WaitAUX(u16 timeout)
{
	while (timeout--)
	{
		if (LORA_GetAUX())
		{
			return 1;
		}
		delay_ms(1);
	}
	return 0;
}

/**
 * @brief 等待模块串口应答数据（USART3 接收完成一帧数据）
 *
 * @param timeout 等待超时时间，单�?ms
 * @return 1: 接收完成  0: 等待超时
 */
static u8 LORA_WaitResponse(u16 timeout)
{
	while (timeout--)
	{
		/*
		USART3 接收完成一帧数据后触发 IDLE 中断�?
		中断处理函数�?usart3_value.flag 标志位置 1
		*/
		if (usart3_value.flag)
		{
			return 1;
		}
		delay_ms(1);
	}
	return 0;
}

/**
 * @brief 连续发�?3 次相同命令字并等待模块应�?
 *			�?xC1�?xC3�?xC4�?xC5�?xC9 命令均为连续发�?3 次格式）
 *
 * @param cmd 需要发送的命令�?
 * @return 1: 收到模块应答  0: 等待应答超时
 */
static u8 LORA_SendTripleCmd(u8 cmd)
{
	u8 buffer[3];

	buffer[0] = cmd;
	buffer[1] = cmd;
	buffer[2] = cmd;

	// 1. 清空 USART3 接收缓冲区，准备接收模块应答
	memset(&usart3_value, 0, sizeof(USART3_Data));

	// 2. 连续发�?3 次相同命令字
	USART3_SendBuffer(buffer, 3);

	// 3. 等待模块应答
	return LORA_WaitResponse(LORA_RESP_WAIT_TIMEOUT);
}

/**
 * @brief LoRa 模块初始化函数，完成 USART3、MD0、MD1、AUX 引脚配置�?
 *			初始化完成后模块进入一般工作状态（透明传输�?
 *
 * @param brr 模块串口工作波特率，出厂默认 9600bps�?
 *				需要与模块 SPEED 寄存器配置的串口波特率保持一�?
 */
void LORA_Init(u32 brr)
{
	GPIO_InitTypeDef gpio_init_struct;

	// 1. USART3 模块初始化，LoRa 模块数据收发通过 USART3 完成
	USART3_Init(brr);

	// 2. RCC 时钟使能 GPIOB 模块（USART3_Init 内部已经使能，重复调用无影响�?
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

	/*
	3. MD0、MD1 引脚配置
		模块内部 MD0（引�?2）、MD1（引�?3�?已经上拉 50K 电阻并串�?100Ω 电阻�?
		按照《AS32-TTL系列操作指南》要求，
		MCU 连接 MD0、MD1 �?IO 配置为开漏输出模�?
	*/
	gpio_init_struct.GPIO_Pin   = LORA_MD0_PIN | LORA_MD1_PIN;
	gpio_init_struct.GPIO_Mode  = GPIO_Mode_Out_OD;
	gpio_init_struct.GPIO_Speed = GPIO_Speed_2MHz;
	GPIO_Init(LORA_GPIO_PORT, &gpio_init_struct);

	// 4. AUX 引脚配置，模�?AUX（引�?4�?默认为推挽输出，MCU 浮空输入即可
	gpio_init_struct.GPIO_Pin  = LORA_AUX_PIN;
	gpio_init_struct.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(LORA_GPIO_PORT, &gpio_init_struct);

	/*
	5. 模块上电后会进行一次自检，自检过程�?AUX 一直保持低电平�?
		等待自检完成后，将模块设置为一般工作状态（透明传输�?
	*/
	delay_ms(100);
	LORA_SetMode(LORA_MODE_NORMAL);
}

/**
 * @brief 设置 LoRa 模块工作状态（MD0、MD1 引脚电平组合�?
 *
 * @param mode 工作状态编码，对应 LORA_MODE_XXX 宏定�?
 */
void LORA_SetMode(u8 mode)
{
	// 1. 检测模块当前处于空闲状态（AUX 高电平）后才能切换工作状�?
	LORA_WaitAUX(LORA_AUX_WAIT_TIMEOUT);
	delay_ms(2);

	// 2. 根据工作状态编码设�?MD0、MD1 引脚电平
	//typedef enum{
	//Bit_RESET = 0,
	//Bit_SET 
	//} BitAction;
	GPIO_WriteBit(LORA_GPIO_PORT, LORA_MD0_PIN, (BitAction)(mode & 0x01));
	GPIO_WriteBit(LORA_GPIO_PORT, LORA_MD1_PIN, (BitAction)((mode >> 1) & 0x01));

	/*
	3. 模块进行状态切换，切换过程�?AUX 输出低电平，
		等待 AUX 重新输出高电平并保持�?2ms，表示状态切换完�?
	*/
	delay_ms(3);
	//这里不进行判断返回值是因为这里的返回值不能确定是成功了还是失败了�?
	//不具有参考价值（切换不进行的话，处于空闲状态也为高点平�?
	LORA_WaitAUX(LORA_AUX_WAIT_TIMEOUT);
	delay_ms(2);
}

/**
 * @brief 获取模块 AUX 引脚当前电平状�?
 *
 * @return 1: AUX 高电平，模块空闲  0: AUX 低电平，模块繁忙
 */
u8 LORA_GetAUX(void)
{
	return GPIO_ReadInputDataBit(LORA_GPIO_PORT, LORA_AUX_PIN);
}

/**
 * @brief 设置模块配置参数�?xC0 命令，配置的参数断电后可保存�?
 *
 * @param config 模块参数结构体指�?
 * @return 1: 配置成功（模块应�?OK�? 0: 配置失败
 */
u8 LORA_SetConfig(const LORA_Config *config)//传入的是指针
/*
   typedef struct lora_config
   {
   	  u8 addh;   // 模块地址�?8 �?
   	  u8 addl;   // 模块地址�?8 �?
   	  u8 speed;  // 通信配置寄存器（串口校验 + 串口波特�?+ 空中速率�?
   	  u8 chan;   // 通信信道寄存器，通信频率 = 410 + CHAN * 1M
   	  u8 option; // 特殊功能寄存器（定点使能 + IO 驱动方式 + 唤醒时间�?
   } LORA_Config;
*/
{
	u8 cmd[6];
	u8 result = 0;

	/*
	1. 切换到休眠工作状�?
		【重点】参数配置命令只支持在休眠工作状态下进行参数的修改，
		�?MD0 = 1，MD1 = 1，且串口固定�?9600bps
	*/
	LORA_SetMode(LORA_MODE_SLEEP);//所有的参数配置命令只能在休眠模式下才能配置，所以配置要先改为休眠模式�?

	// 2. 清空 USART3 接收缓冲区，准备接收模块应答
	/*
	   typedef struct {
	   u8  data[256];   // 接收内容
	   u16 count;       // 有效字节�?
	   u8  flag;        // 接收完成标志
       } USART3_Data;
	*/
	memset(&usart3_value, 0, sizeof(USART3_Data));//防止之前的数据没有清�?

	// 3. 按照命令格式组帧�?xC0 ADDH ADDL SPEED CHAN OPTION
	cmd[0] = LORA_CMD_SET_CONFIG;
	cmd[1] = config->addh;
	cmd[2] = config->addl;
	cmd[3] = config->speed;
	cmd[4] = config->chan;
	cmd[5] = config->option;//将每个命令放入数组中

	USART3_SendBuffer(cmd, 6);

	/*
	4. 等待并判断模块应�?
		配置成功：模块返�?ASCII 字符�?"OK"，参数断电保�?
		配置失败：模块返�?ASCII 字符�?"ERROR"，原配置参数未被更改
	*/
	if (LORA_WaitResponse(LORA_RESP_WAIT_TIMEOUT))
	{
if (LORA_MemFind(usart3_value.data, usart3_value.count, "OK"))
		{
			result = 1;
		}
	}

	// 5. 配置完成，返回一般工作状态（透明传输�?
	LORA_SetMode(LORA_MODE_NORMAL);

	return result;
}

/**
 * @brief 读取模块当前配置参数�?xC1 命令�?
 *
 * @param config 模块参数结构体指针，用于存放读取结果
 * @return 1: 读取成功  0: 读取失败
 */
u8 LORA_ReadConfig(LORA_Config *config)
{
	u8 result = 0;//默认是失败的，result 是返回�?

	// 1. 切换到休眠工作状�?
	LORA_SetMode(LORA_MODE_SLEEP);

	// 2. 发�?0xC1 0xC1 0xC1 命令并等待模块应�?
	if (LORA_SendTripleCmd(LORA_CMD_READ_CONFIG))
	{
		/*
		模块以十六进制格式返回当前的配置参数�?
			0xC0 ADDH ADDL SPEED CHAN OPTION �?6 字节
			例如出厂默认：C0 00 00 1A 3D 40
		*/
		if ((usart3_value.count >= 6) && (usart3_value.data[0] == LORA_CMD_SET_CONFIG))
		{
			config->addh   = usart3_value.data[1];
			config->addl   = usart3_value.data[2];
			config->speed  = usart3_value.data[3];
			config->chan   = usart3_value.data[4];
			config->option = usart3_value.data[5];

			result = 1;
		}
	}

	// 3. 读取完成，返回一般工作状�?
	LORA_SetMode(LORA_MODE_NORMAL);

	return result;
}

/**
 * @brief 复位模块�?xC4 命令�?
 *
 * @return 1: 复位成功  0: 复位失败
 */
u8 LORA_Reset(void)
{
	u8 result = 0;//默认是失败的，result 是返回�?

	// 1. 切换到休眠工作状�?
	LORA_SetMode(LORA_MODE_SLEEP);

	// 2. 发�?0xC4 0xC4 0xC4 命令并等待模块应�?"OK"
	if (LORA_SendTripleCmd(LORA_CMD_RESET))
	{
if (LORA_MemFind(usart3_value.data, usart3_value.count, "OK"))
		{
			result = 1;
		}
	}

	/*
	3. 模块产生一次复位，复位过程中模块进行自检，AUX 输出低电平，
		复位完毕 AUX 输出高电平，模块开始正常工�?
	*/
	LORA_SetMode(LORA_MODE_NORMAL);
	delay_ms(500);

	return result;
}

/**
 * @brief 读取模块的实际电压（0xC5 命令�?
 *
 * @param voltage 用于存放电压值，单位 mV，例�?3100 表示 3.1V
 * @return 1: 读取成功  0: 读取失败
 */
u8 LORA_ReadVoltage(u16 *voltage)
{
	u8 result = 0;//默认是失败的，result 是返回�?

	// 1. 切换到休眠工作状�?
	LORA_SetMode(LORA_MODE_SLEEP);

	// 2. 发�?0xC5 0xC5 0xC5 命令并等待模块应�?
	if (LORA_SendTripleCmd(LORA_CMD_READ_VOLTAGE))
	{
		/*
		模块返回 C5 VH（高字节�?VL（低字节�?�?3 字节�?
		(VH << 8 | VL) 即为电压值，单位 mV （大端写法）
		例如模块返回 C5 0C 1C�?x0C1C = 3100，表示当前电压为 3.1V
		*/
		if ((usart3_value.count >= 3) && (usart3_value.data[0] == LORA_CMD_READ_VOLTAGE))
		{
			*voltage = ((u16)usart3_value.data[1] << 8) | usart3_value.data[2];

			result = 1;
		}
	}

	// 3. 读取完成，返回一般工作状�?
	LORA_SetMode(LORA_MODE_NORMAL);

	return result;
}



/**
 * @brief 读取模块硬件版本号（0xC3 命令�?
 *
 * @param version 用于存放版本号字符串的缓冲区
 * @param max_len 缓冲区总字节数（需 >= 15�?
 * @return 1: 读取成功  0: 读取失败
 */
u8 LORA_ReadVersion(char *version, u16 max_len)
{
	u8  result = 0;
	u16 n;

	/* 参数防御：缓冲区太小直接拒绝，避免溢�?*/
	//可以不要
	if ((version == NULL) || (max_len < 32))
	{
		return 0;
	}

	/* 1. 切换到休眠工作状态（参数命令必须在休眠下执行�?*/
	LORA_SetMode(LORA_MODE_SLEEP);

	/* 2. 发�?0xC3 0xC3 0xC3 并等待应�?*/
	if (LORA_SendTripleCmd(LORA_CMD_READ_VERSION))
	{
		/*
		模块�?ASCII 格式返回硬件版本号，形如�?
			AS32-TTL-100-C-V9.0
		首字节是 'A'，不是命令码 0xC3
		*/
		if ((usart3_value.count >= 14) &&
			(usart3_value.data[0] == 'A') &&
			(usart3_value.data[1] == 'S'))
		{
			n = usart3_value.count;

			/* �?1 字节�?'\0' */
			if (n > (max_len - 1))
			{
				n = max_len - 1;
			}

			memcpy(version, usart3_value.data, n);
			version[n] = '\0';

			result = 1;
		}
	}

	/* 3. 读取完成，返回一般工作状�?*/
	LORA_SetMode(LORA_MODE_NORMAL);

	return result;
}



/**
 * @brief 透明传输模式发送数�?
 *			【重点】模块内部具�?256 字节缓存 FIFO，单次发送不要超�?256 字节
 *
 * @param buffer 发送数据对应内存空间首地址
 * @param len    发送目标数据字节数
 */
void LORA_SendData(u8 *buffer, u16 len)
{
	/*
	 * [FIX-DBG3] ԭ����ֻ���� LORA_WaitAUX() �����Է���ֵ��
	 *   �� AUX ����δ�ӣ�PB14 ���գ���ģ���쳣��AUX ����һֱ�����͵�ƽ��
	 *   ÿ�η��Ͷ�Ҫ�׵��� LORA_AUX_WAIT_TIMEOUT(500ms)����ѭ�����ؿ���
	 *   ��PC->LoRa ͸��ʱ�������ԣ�ÿ���ֽڶ����ܴ�������
	 *   ���ڸ�Ϊ��
	 *     - �ȴ�ʱ���ý϶̵� LORA_AUX_SEND_TIMEOUT(50ms)��
	 *       ����ģ�鷢��һ֡Ҳ�ͼ����뵽��ʮ���룬50ms �㹻��
	 *     - ��ʱҲ�������ͣ��������ݣ���ֻ�ǲ��ٳ�ʱ��������
	 */
	if (buffer == 0 || len == 0)
	{
		return;
	}

	LORA_WaitAUX(LORA_AUX_SEND_TIMEOUT);

	USART3_SendBuffer(buffer, len);
}

/**
 * @brief 定点传输模式发送数�?
 *			【重点】需要先�?OPTION[7] 配置�?1 开启定点传输模式，
 *			发送方与接收方的地址、信道可以不相同
 *
 * @param address 接收方模块地址�?x0000 ~ 0xFFFF�?
 * @param chan    接收方模块信�?
 * @param buffer  发送数据对应内存空间首地址
 * @param len     发送目标数据字节数
 */
void LORA_FixedSend(u16 address, u8 chan, u8 *buffer, u16 len)
{
	u8 head[3];

	/*
	定点传输发送方数据格式�?
		接收方地址�?8 �?+ 接收方地址�?8 �?+ 接收方信�?+ 用户数据
	若前 3 字节�?0xFF 0xFF + 信道，则为定点传输下的广播，
		处于同一信道下的模块均可接收数据
	*/
	head[0] = (u8)(address >> 8);
	head[1] = (u8)(address & 0xFF);
	head[2] = chan;

	LORA_WaitAUX(LORA_AUX_WAIT_TIMEOUT);

	USART3_SendBuffer(head, 3);
	USART3_SendBuffer(buffer, len);
}
