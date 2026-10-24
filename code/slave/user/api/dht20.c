#include "dht20.h"

/* 奥松 DHT20 温湿度传感器驱动（软件模拟 I2C）
 * 依据《DHT20 产品规格书》V1.0（2021-05）
 */

/* ---------------- 延时（Cortex-M3 DWT 周期计数器） ---------------- */

#define DHT20_DEMCR            (*(volatile uint32_t *)0xE000EDFCUL)
#define DHT20_DWT_CTRL         (*(volatile uint32_t *)0xE0001000UL)
#define DHT20_DWT_CYCCNT       (*(volatile uint32_t *)0xE0001004UL)
#define DHT20_DWT_TRCENA       (1UL << 24)
#define DHT20_DWT_CYCCNTENA    (1UL << 0)

static uint8_t s_dwt_state = 0;            /* 0=未检测 1=可用 2=不可用 */

static uint8_t DHT20_DWT_Check(void)
{
    uint32_t a;
    uint32_t b;
    uint32_t i;

    if (s_dwt_state != 0)
    {
        return (s_dwt_state == 1) ? 1 : 0;
    }

    DHT20_DEMCR |= DHT20_DWT_TRCENA;
    DHT20_DWT_CYCCNT = 0;
    DHT20_DWT_CTRL |= DHT20_DWT_CYCCNTENA;

    a = DHT20_DWT_CYCCNT;
    for (i = 0; i < 2000; i++)
    {
        __NOP();
    }
    b = DHT20_DWT_CYCCNT;

    s_dwt_state = (b != a) ? 1 : 2;

    return (s_dwt_state == 1) ? 1 : 0;
}

static void DHT20_Delay_Loop(uint32_t n)
{
    uint32_t k;

    while (n--)
    {
        for (k = 0; k < 2; k++)
        {
            ;
        }
    }
}

void DHT20_Delay_us(uint32_t t)
{
    uint32_t ticks;
    uint32_t start;

    if (DHT20_DWT_Check())
    {
        ticks = t * (SystemCoreClock / 1000000UL);

        if (ticks == 0)
        {
            ticks = 1;
        }

        start = DHT20_DWT_CYCCNT;
        while ((DHT20_DWT_CYCCNT - start) < ticks)
        {
            ;
        }
    }
    else
    {
        DHT20_Delay_Loop(t);
    }
}

void DHT20_Delay_ms(uint32_t t)
{
    uint32_t ticks;
    uint32_t start;

    if (DHT20_DWT_Check())
    {
        ticks = t * (SystemCoreClock / 1000UL);

        if (ticks == 0)
        {
            ticks = 1;
        }

        start = DHT20_DWT_CYCCNT;
        while ((DHT20_DWT_CYCCNT - start) < ticks)
        {
            ;
        }
    }
    else
    {
        DHT20_Delay_Loop(t * 1000UL);
    }
}

/* ---------------------------- GPIO ---------------------------- */

static void DHT20_SDA_Mode_Out(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    GPIO_InitStructure.GPIO_Pin   = DHT20_SDA_PIN;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_OD;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(DHT20_GPIO_PORT, &GPIO_InitStructure);
}

static void DHT20_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(DHT20_GPIO_CLK, ENABLE);

    GPIO_InitStructure.GPIO_Pin   = DHT20_SCL_PIN | DHT20_SDA_PIN;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_OD;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(DHT20_GPIO_PORT, &GPIO_InitStructure);

    GPIO_SetBits(DHT20_GPIO_PORT, DHT20_SCL_PIN | DHT20_SDA_PIN);
}

/* 读操作会把 SDA 切成输入，写之前必须切回开漏输出，否则置位无效 */
static void DHT20_SDA_Out_High(void)
{
    DHT20_SDA_Mode_Out();
    GPIO_SetBits(DHT20_GPIO_PORT, DHT20_SDA_PIN);
}

static void DHT20_SDA_Out_Low(void)
{
    DHT20_SDA_Mode_Out();
    GPIO_ResetBits(DHT20_GPIO_PORT, DHT20_SDA_PIN);
}

static void DHT20_SDA_In(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    GPIO_InitStructure.GPIO_Pin  = DHT20_SDA_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(DHT20_GPIO_PORT, &GPIO_InitStructure);
}

static void DHT20_SCL_Out_High(void)
{
    GPIO_SetBits(DHT20_GPIO_PORT, DHT20_SCL_PIN);
}

static void DHT20_SCL_Out_Low(void)
{
    GPIO_ResetBits(DHT20_GPIO_PORT, DHT20_SCL_PIN);
}

/* --------------------------- I2C 时序 --------------------------- */

static void DHT20_I2C_Start(void)
{
    DHT20_SDA_Out_High();
    DHT20_Delay_us(4);
    DHT20_SCL_Out_High();
    DHT20_Delay_us(4);
    DHT20_SDA_Out_Low();
    DHT20_Delay_us(4);
    DHT20_SCL_Out_Low();
    DHT20_Delay_us(4);
}

static void DHT20_I2C_Stop(void)
{
    DHT20_SCL_Out_Low();
    DHT20_Delay_us(4);
    DHT20_SDA_Out_Low();
    DHT20_Delay_us(4);
    DHT20_SCL_Out_High();
    DHT20_Delay_us(4);
    DHT20_SDA_Out_High();
    DHT20_Delay_us(4);
}

static void DHT20_Write_Byte(uint8_t byte)
{
    uint8_t i;

    for (i = 0; i < 8; i++)
    {
        DHT20_SCL_Out_Low();
        DHT20_Delay_us(5);

        if (byte & 0x80)
        {
            DHT20_SDA_Out_High();
        }
        else
        {
            DHT20_SDA_Out_Low();
        }

        DHT20_Delay_us(5);
        DHT20_SCL_Out_High();
        DHT20_Delay_us(4);
        byte <<= 1;
    }

    DHT20_SCL_Out_Low();
    DHT20_Delay_us(5);
}

static uint8_t DHT20_Read_Byte(void)
{
    uint8_t i;
    uint8_t byte = 0;

    DHT20_SDA_In();

    for (i = 0; i < 8; i++)
    {
        byte <<= 1;

        DHT20_SCL_Out_Low();
        DHT20_Delay_us(5);
        DHT20_SCL_Out_High();
        DHT20_Delay_us(5);

        if (GPIO_ReadInputDataBit(DHT20_GPIO_PORT, DHT20_SDA_PIN))
        {
            byte |= 0x01;
        }

        DHT20_Delay_us(5);
    }

    DHT20_SCL_Out_Low();
    DHT20_Delay_us(5);

    return byte;
}

static uint8_t DHT20_Wait_Ack(void)
{
    uint16_t cnt = 0;
    uint8_t  ack = 0;

    DHT20_SDA_In();
    DHT20_Delay_us(5);
    DHT20_SCL_Out_Low();
    DHT20_Delay_us(5);
    DHT20_SCL_Out_High();
    DHT20_Delay_us(5);

    while (GPIO_ReadInputDataBit(DHT20_GPIO_PORT, DHT20_SDA_PIN))
    {
        cnt++;
        if (cnt > 200)
        {
            break;
        }
    }

    if (cnt <= 200)
    {
        ack = 1;
    }

    DHT20_SCL_Out_Low();
    DHT20_Delay_us(5);

    return ack;
}

static void DHT20_Send_Ack(void)
{
    DHT20_SCL_Out_Low();
    DHT20_Delay_us(5);
    DHT20_SDA_Out_Low();
    DHT20_Delay_us(5);
    DHT20_SCL_Out_High();
    DHT20_Delay_us(5);
    DHT20_SCL_Out_Low();
    DHT20_Delay_us(5);
    DHT20_SDA_In();
}

static void DHT20_Send_NAck(void)
{
    DHT20_SCL_Out_Low();
    DHT20_Delay_us(5);
    DHT20_SDA_Out_High();
    DHT20_Delay_us(5);
    DHT20_SCL_Out_High();
    DHT20_Delay_us(5);
    DHT20_SCL_Out_Low();
    DHT20_Delay_us(5);
}

/* 从机被中断后可能一直拉着 SDA，发 9 个时钟冲开再补停止信号 */
static void DHT20_Bus_Recover(void)
{
    uint8_t i;

    DHT20_SDA_Out_High();
    DHT20_Delay_us(4);

    for (i = 0; i < 9; i++)
    {
        DHT20_SCL_Out_Low();
        DHT20_Delay_us(4);
        DHT20_SCL_Out_High();
        DHT20_Delay_us(4);
    }

    DHT20_I2C_Stop();
    DHT20_Delay_us(10);
}

/* --------------------------- DHT20 命令 --------------------------- */

static uint8_t DHT20_Calc_CRC8(uint8_t *message, uint8_t num)
{
    uint8_t i;
    uint8_t byte;
    uint8_t crc = 0xFF;

    for (byte = 0; byte < num; byte++)
    {
        crc ^= message[byte];

        for (i = 0; i < 8; i++)
        {
            if (crc & 0x80)
            {
                crc <<= 1;
                crc ^= 0x31;
            }
            else
            {
                crc <<= 1;
            }
        }
    }

    return crc;
}

/*
 * [FIX-BUG1] 原实现用 0xFF 作为"读失败"哨兵值，但
 *            0xFF & DHT20_STATUS_CAL(0x18) 恰好等于 0x18，
 *            即"读失败"会被调用方误判为"已校准"，
 *            导致 DHT20_Init() 里的校准初始化被永久跳过，读数持续偏差。
 *            现改为：返回值只表示成功(1)/失败(0)，状态字节由输出参数带回，
 *            错误与数据彻底分离。
 */
static uint8_t DHT20_Read_Status(uint8_t *pStatus)
{
    uint8_t byte;

    if (pStatus == 0)
    {
        return 0;
    }

    DHT20_I2C_Start();
    DHT20_Write_Byte(DHT20_ADDR_R);
    if (!DHT20_Wait_Ack())
    {
        DHT20_I2C_Stop();
        return 0;
    }
    byte = DHT20_Read_Byte();
    DHT20_Send_NAck();
    DHT20_I2C_Stop();

    *pStatus = byte;
    return 1;
}

static uint8_t DHT20_Trigger_Measure(void)
{
    uint8_t ok = 0;

    DHT20_I2C_Start();
    DHT20_Write_Byte(DHT20_ADDR_W);

    if (DHT20_Wait_Ack())
    {
        DHT20_Write_Byte(DHT20_CMD_TRIGGER);
        if (DHT20_Wait_Ack())
        {
            DHT20_Write_Byte(DHT20_CMD_TRIGGER_P1);
            if (DHT20_Wait_Ack())
            {
                DHT20_Write_Byte(DHT20_CMD_TRIGGER_P2);
                if (DHT20_Wait_Ack())
                {
                    ok = 1;
                }
            }
        }
    }

    DHT20_I2C_Stop();

    return ok;
}

static void DHT20_Reset_Register(uint8_t addr)
{
    uint8_t buf[3];
    uint8_t i;

    DHT20_I2C_Start();
    DHT20_Write_Byte(DHT20_ADDR_W);
    if (!DHT20_Wait_Ack())
    {
        DHT20_I2C_Stop();
        return;
    }
    DHT20_Write_Byte(addr);
    DHT20_Wait_Ack();
    DHT20_Write_Byte(0x00);
    DHT20_Wait_Ack();
    DHT20_Write_Byte(0x00);
    DHT20_Wait_Ack();
    DHT20_I2C_Stop();
    DHT20_Delay_ms(10);

    DHT20_I2C_Start();
    DHT20_Write_Byte(DHT20_ADDR_R);
    if (!DHT20_Wait_Ack())
    {
        DHT20_I2C_Stop();
        return;
    }
    for (i = 0; i < 3; i++)
    {
        buf[i] = DHT20_Read_Byte();
        if (i < 2)
        {
            DHT20_Send_Ack();
        }
        else
        {
            DHT20_Send_NAck();
        }
    }
    DHT20_I2C_Stop();
    DHT20_Delay_ms(10);

    DHT20_I2C_Start();
    DHT20_Write_Byte(DHT20_ADDR_W);
    if (!DHT20_Wait_Ack())
    {
        DHT20_I2C_Stop();
        return;
    }
    DHT20_Write_Byte(0xB0 | addr);
    DHT20_Wait_Ack();
    DHT20_Write_Byte(buf[1]);
    DHT20_Wait_Ack();
    DHT20_Write_Byte(buf[2]);
    DHT20_Wait_Ack();
    DHT20_I2C_Stop();
}

static void DHT20_Start_Init(void)
{
    DHT20_Reset_Register(0x1B);
    DHT20_Delay_ms(10);
    DHT20_Reset_Register(0x1C);
    DHT20_Delay_ms(10);
    DHT20_Reset_Register(0x1E);
    DHT20_Delay_ms(10);
}

/* --------------------------- 对外接口 --------------------------- */

void DHT20_Init(void)
{
    DHT20_Delay_ms(DHT20_POWER_MS);
    DHT20_GPIO_Init();
    DHT20_Delay_ms(DHT20_STARTUP_MS);
    DHT20_Bus_Recover();

    uint8_t status = 0;

    /*
     * [FIX-BUG1] 原写法 (DHT20_Read_Status() & DHT20_STATUS_CAL) != DHT20_STATUS_CAL
     *            在"读状态失败"时会被误判为已校准（0xFF & 0x18 == 0x18），
     *            导致校准初始化被永久跳过。
     *            现在：读成功且确实已校准才跳过；读失败时保守地执行一次校准初始化。
     */
    if (!DHT20_Read_Status(&status) || ((status & DHT20_STATUS_CAL) != DHT20_STATUS_CAL))
    {
        DHT20_Start_Init();
    }
}

uint8_t DHT20_Read_Raw(uint32_t *hum_raw, uint32_t *temp_raw)
{
    uint8_t buf[7];
    uint8_t i;
    uint8_t ready = 0;

    DHT20_Delay_ms(DHT20_TRIGGER_MS);

    if (!DHT20_Trigger_Measure())
    {
        DHT20_Bus_Recover();
        return DHT20_ERR_TRIG;
    }

    DHT20_Delay_ms(DHT20_MEASURE_MS);

    for (i = 0; i < DHT20_POLL_TIMES; i++)
    {
        uint8_t status = 0;

        /* [FIX-BUG1] 必须先判读状态是否成功，读失败不能当成"不忙" */
        if (DHT20_Read_Status(&status) && ((status & DHT20_STATUS_BUSY) == 0))
        {
            ready = 1;
            break;
        }
        DHT20_Delay_ms(DHT20_POLL_GAP_MS);
    }

    if (!ready)
    {
        DHT20_Bus_Recover();
        return DHT20_ERR_STATUS;
    }

    DHT20_I2C_Start();
    DHT20_Write_Byte(DHT20_ADDR_R);
    if (!DHT20_Wait_Ack())
    {
        DHT20_I2C_Stop();
        DHT20_Bus_Recover();
        return DHT20_ERR_READ;
    }

    for (i = 0; i < 6; i++)
    {
        buf[i] = DHT20_Read_Byte();
        DHT20_Send_Ack();
    }

    buf[6] = DHT20_Read_Byte();
    DHT20_Send_NAck();
    DHT20_I2C_Stop();

    if (DHT20_Calc_CRC8(buf, 6) != buf[6])
    {
        return DHT20_ERR_CRC;
    }

    /* buf[1]~buf[5]：高 20bit 湿度，低 20bit 温度 */
    *hum_raw  = ((uint32_t)buf[1] << 12) | ((uint32_t)buf[2] << 4) | ((uint32_t)buf[3] >> 4);
    *temp_raw = (((uint32_t)buf[3] & 0x0F) << 16) | ((uint32_t)buf[4] << 8) | (uint32_t)buf[5];

    return DHT20_OK;
}

uint8_t DHT20_Read(float *humidity, float *temperature)
{
    uint32_t hum_raw  = 0;
    uint32_t temp_raw = 0;
    uint8_t  ret;

    ret = DHT20_Read_Raw(&hum_raw, &temp_raw);
    if (ret != DHT20_OK)
    {
        return ret;
    }

    *humidity    = (float)hum_raw  * 100.0f / 1048576.0f;
    *temperature = (float)temp_raw * 200.0f / 1048576.0f - 50.0f;

    return DHT20_OK;
}
