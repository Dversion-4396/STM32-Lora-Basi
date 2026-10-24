#include "master_app.h"
#include "oled.h"
#include "i2c.h"
#include "delay.h"

/*
================================================================================
【实现说明】LoRa 主机：帧解析 + OLED 显示
--------------------------------------------------------------------------------
    1. 帧解析不用 sscanf / strtod（会拉进大量库代码、还可能越界读），
       全部用带长度边界的指针扫描，逐字段定位 "KEY=" 后取值。
    2. 帧不是 NUL 结尾的，所有查找都必须在 [0, len) 范围内，绝不能 strstr。
    3. 校验和必须验：对 "*" 之前所有字节累加取低 8 位，和帧尾两字节 HEX 比对，
       不一致就丢弃，避免 LoRa 误码显示成错误数据。
    4. OLED 每行占 2 页（16px 高），4 行分别用 page 0 / 2 / 4 / 6。
    5. 【编码】本文件必须保存为 GBK，因为 OLED_ShowString 按 GBK 解码汉字，
       UTF-8 编码的中文字面量会显示成乱码。
================================================================================
*/

/* 主机全局数据 */
MasterData_t master_data;

/* 帧头，必须完全匹配 */
static const char *FRAME_HEAD = "$RAIN,";

/* 显示缓冲：一行最多 20 个 ASCII 字符，留足 24 字节 */
static char s_line[24];

/* ---------------------------------------------------------------------------
 * 在 [0, len) 范围内查找子串，找到返回下标，找不到返回 -1
 * 不用 strstr：帧没有 NUL 结尾，strstr 会越界读
 * ------------------------------------------------------------------------- */
static int32_t FindSub(const uint8_t *buf, uint16_t len, const char *pat, uint16_t patLen)
{
    uint16_t i;

    if (len < patLen)
    {
        return -1;
    }

    for (i = 0; i + patLen <= len; i++)
    {
        uint16_t j;
        for (j = 0; j < patLen; j++)
        {
            if (buf[i + j] != (uint8_t)pat[j])
            {
                break;
            }
        }
        if (j == patLen)
        {
            return (int32_t)i;
        }
    }

    return -1;
}

/* 计算字符串长度（不含 NUL），自己写一份避免额外依赖 */
static uint16_t StrLen(const char *s)
{
    uint16_t n = 0;
    while (s[n] != 0)
    {
        n++;
    }
    return n;
}

/* ---------------------------------------------------------------------------
 * 从 buf[pos] 开始解析一个定点小数（如 "27.5" 或 "-3.2"）
 * 返回值已乘 10（27.5 -> 275），解析失败返回 0 并置 *ok = 0
 * "--" 表示无效数据，会返回 *ok = 0
 * ------------------------------------------------------------------------- */
static int32_t ParseFixed1(const uint8_t *buf, uint16_t len, uint16_t pos, uint8_t *ok)
{
    int32_t sign    = 1;
    int32_t intPart = 0;
    int32_t frac    = 0;
    uint8_t digits  = 0;

    *ok = 0;

    if (pos >= len)
    {
        return 0;
    }

    /* 负号；"--" 是无效标记，不是负数 */
    if (buf[pos] == '-')
    {
        if ((pos + 1 < len) && (buf[pos + 1] == '-'))
        {
            return 0;
        }
        sign = -1;
        pos++;
    }

    /* 整数部分 */
    while ((pos < len) && (buf[pos] >= '0') && (buf[pos] <= '9'))
    {
        intPart = intPart * 10 + (buf[pos] - '0');
        pos++;
        digits++;
    }

    if (digits == 0)
    {
        return 0;
    }

    /* 小数部分，只取一位 */
    if ((pos < len) && (buf[pos] == '.'))
    {
        pos++;
        if ((pos < len) && (buf[pos] >= '0') && (buf[pos] <= '9'))
        {
            frac = buf[pos] - '0';
            pos++;
        }
    }

    *ok = 1;
    return sign * (intPart * 10 + frac);
}

/* ---------------------------------------------------------------------------
 * 从 buf[pos] 开始解析一个无符号整数，结束于非数字字符
 * ------------------------------------------------------------------------- */
static uint32_t ParseUInt(const uint8_t *buf, uint16_t len, uint16_t pos, uint8_t *ok)
{
    uint32_t v = 0;
    uint8_t  digits = 0;

    *ok = 0;

    while ((pos < len) && (buf[pos] >= '0') && (buf[pos] <= '9'))
    {
        v = v * 10 + (uint32_t)(buf[pos] - '0');
        pos++;
        digits++;
    }

    if (digits == 0)
    {
        return 0;
    }

    *ok = 1;
    return v;
}

/* 累加校验和：对 [0, len) 所有字节求和取低 8 位 */
static uint8_t CheckSum(const uint8_t *buf, uint16_t len)
{
    uint16_t i;
    uint8_t  sum = 0;

    for (i = 0; i < len; i++)
    {
        sum += buf[i];
    }

    return sum;
}

/* HEX 字符转数值，非法返回 -1 */
static int8_t HexVal(uint8_t c)
{
    if ((c >= '0') && (c <= '9'))
    {
        return (int8_t)(c - '0');
    }
    if ((c >= 'A') && (c <= 'F'))
    {
        return (int8_t)(c - 'A' + 10);
    }
    if ((c >= 'a') && (c <= 'f'))
    {
        return (int8_t)(c - 'a' + 10);
    }
    return -1;
}

/**
 * @brief 解析从机发来的一帧
 */
uint8_t Master_ParseFrame(const uint8_t *frame, uint16_t len)
{
    int32_t  starIdx;
    int32_t  headIdx;
    uint8_t  sumCalc;
    uint8_t  sumRecv;
    int8_t   h1, h2;
    uint8_t  ok;
    uint16_t headLen;
    uint16_t bodyLen;

    if ((frame == NULL) || (len < 8))
    {
        return 0;
    }

    /* 1. 帧头必须是 "$RAIN," */
    headLen = StrLen(FRAME_HEAD);
    headIdx = FindSub(frame, len, FRAME_HEAD, headLen);
    if (headIdx < 0)
    {
        return 0;   /* 不是本工程的帧，直接丢弃 */
    }

    /* 2. 找校验和起始符 '*' */
    starIdx = -1;
    {
        uint16_t i;
        for (i = (uint16_t)headIdx; i < len; i++)
        {
            if (frame[i] == '*')
            {
                starIdx = (int32_t)i;
                break;
            }
        }
    }
    if (starIdx < 0)
    {
        return 0;   /* 没有校验和，帧不完整 */
    }

    /* 3. "*" 后必须还有 2 个 HEX 字符 */
    if ((uint16_t)(starIdx + 3) > len)
    {
        return 0;
    }

    h1 = HexVal(frame[starIdx + 1]);
    h2 = HexVal(frame[starIdx + 2]);
    if ((h1 < 0) || (h2 < 0))
    {
        return 0;
    }

    /* 4. 校验和比对 */
    sumCalc = CheckSum(frame, (uint16_t)starIdx);
    sumRecv = (uint8_t)(((uint8_t)h1 << 4) | (uint8_t)h2);
    if (sumCalc != sumRecv)
    {
        return 0;   /* 校验失败，丢弃 */
    }

    /* 字段解析范围限定在 [0, starIdx) */
    bodyLen = (uint16_t)starIdx;

    /*
     * 5. 逐字段解析
     *    每个字段都是 "KEY=" 的形式，找到 KEY 后从 '=' 之后开始取值。
     *    全部在 [0, bodyLen) 内操作，不会越界。
     */
    {
        int32_t p;
        int32_t t;

        /* ---- 温度 T= ---- */
        p = FindSub(frame, bodyLen, "T=", 2);
        if (p >= 0)
        {
            t = ParseFixed1(frame, bodyLen, (uint16_t)(p + 2), &ok);
            if (ok)
            {
                master_data.temperature = (float)t / 10.0f;
                master_data.tempValid   = 1;
            }
            else
            {
                master_data.tempValid = 0;
            }
        }
        else
        {
            master_data.tempValid = 0;
        }

        /* ---- 湿度 H= ---- */
        p = FindSub(frame, bodyLen, "H=", 2);
        if (p >= 0)
        {
            t = ParseFixed1(frame, bodyLen, (uint16_t)(p + 2), &ok);
            if (ok)
            {
                master_data.humidity  = (float)t / 10.0f;
                master_data.humiValid = 1;
            }
            else
            {
                master_data.humiValid = 0;
            }
        }
        else
        {
            master_data.humiValid = 0;
        }

        /* ---- 雨滴 ADC= ---- */
        p = FindSub(frame, bodyLen, "ADC=", 4);
        if (p >= 0)
        {
            uint32_t v = ParseUInt(frame, bodyLen, (uint16_t)(p + 4), &ok);
            if (ok && (v <= 4095u))
            {
                master_data.rainAdc = (uint16_t)v;
            }
        }

        /* ---- 雨情等级 LEVEL= ---- */
        p = FindSub(frame, bodyLen, "LEVEL=", 6);
        if (p >= 0)
        {
            uint32_t v = ParseUInt(frame, bodyLen, (uint16_t)(p + 6), &ok);
            if (ok && (v <= 3u))
            {
                master_data.rainLevel = (uint8_t)v;
            }
        }

        /* ---- 水位百分比 WL= ---- */
        p = FindSub(frame, bodyLen, "WL=", 3);
        if (p >= 0)
        {
            uint32_t v = ParseUInt(frame, bodyLen, (uint16_t)(p + 3), &ok);
            if (ok && (v <= 100u))
            {
                master_data.waterLevel = (uint8_t)v;
                master_data.waterValid = 1;
            }
            else
            {
                master_data.waterValid = 0;
            }
        }
        else
        {
            /* 从机是老固件，没有 WL 字段 */
            master_data.waterValid = 0;
        }
    }

    /*
     * 6. 水位报警判定
     *    水位 > WATER_ALARM_THRESHOLD 时置报警标志，
     *    供 Master_Display 显示“水位过高”、主循环驱动蜂鸣器。
     *    注意：报警状态每帧重新计算，水位降下来会自动消除。
     */
    if (master_data.waterValid && (master_data.waterLevel > WATER_ALARM_THRESHOLD))
    {
        master_data.waterAlarm = 1;
    }
    else
    {
        master_data.waterAlarm = 0;
    }

    master_data.hasData = 1;
    return 1;
}

/* ---------------------------------------------------------------------------
 * 定点数追加：value10 是已乘 10 的整数（275 -> "27.5"）
 * 返回新的写入位置
 * ------------------------------------------------------------------------- */
static uint8_t LineAppendFixed1(char *out, uint8_t n, int32_t value10)
{
    uint8_t tmp[10];
    uint8_t cnt = 0;
    uint8_t i;
    int32_t v = value10;
    int32_t intPart;
    int32_t frac;

    if (v < 0)
    {
        out[n++] = '-';
        v = -v;
    }

    intPart = v / 10;
    frac    = v % 10;

    if (intPart == 0)
    {
        out[n++] = '0';
    }
    else
    {
        while (intPart > 0)
        {
            tmp[cnt++] = (char)('0' + (intPart % 10));
            intPart /= 10;
        }
        for (i = 0; i < cnt; i++)
        {
            out[n++] = tmp[cnt - 1 - i];
        }
    }

    out[n++] = '.';
    out[n++] = (char)('0' + frac);

    return n;
}

/* 无符号整数追加 */
static uint8_t LineAppendUInt(char *out, uint8_t n, uint32_t v)
{
    uint8_t tmp[10];
    uint8_t cnt = 0;
    uint8_t i;

    if (v == 0)
    {
        out[n++] = '0';
        return n;
    }

    while (v > 0)
    {
        tmp[cnt++] = (char)('0' + (v % 10));
        v /= 10;
    }

    for (i = 0; i < cnt; i++)
    {
        out[n++] = tmp[cnt - 1 - i];
    }

    return n;
}

/* 追加 ASCII 字符串 */
static uint8_t LineAppendStr(char *out, uint8_t n, const char *s)
{
    while (*s != 0)
    {
        out[n++] = *s++;
    }
    return n;
}

/**
 * @brief 把 master_data 刷新到 OLED
 *        4 行：温度 / 湿度 / 水位 / 雨情
 */
void Master_Display(void)
{
    uint8_t n;

    OLED_Clear();

    /* ---- 第 1 行：温度:xx.x ---- */
    n = 0;
    n = LineAppendStr(s_line, n, "温度:");
    if (master_data.tempValid)
    {
        n = LineAppendFixed1(s_line, n, (int32_t)(master_data.temperature * 10.0f + 0.5f));
    }
    else
    {
        n = LineAppendStr(s_line, n, "--");
    }
    s_line[n] = 0;
    OLED_ShowString(0, 0, s_line);

    /* ---- 第 2 行：湿度:xx.x ---- */
    n = 0;
    n = LineAppendStr(s_line, n, "湿度:");
    if (master_data.humiValid)
    {
        n = LineAppendFixed1(s_line, n, (int32_t)(master_data.humidity * 10.0f + 0.5f));
    }
    else
    {
        n = LineAppendStr(s_line, n, "--");
    }
    s_line[n] = 0;
    OLED_ShowString(0, 2, s_line);

    /* ---- 第 3 行：水位:xx（报警时加 ! 强调）---- */
    n = 0;
    n = LineAppendStr(s_line, n, "水位:");
    if (master_data.waterValid)
    {
        n = LineAppendUInt(s_line, n, master_data.waterLevel);
        n = LineAppendStr(s_line, n, "%");
    }
    else
    {
        n = LineAppendStr(s_line, n, "--");
    }
    if (master_data.waterAlarm)
    {
        n = LineAppendStr(s_line, n, "!");
    }
    s_line[n] = 0;
    OLED_ShowString(0, 4, s_line);

    /*
     * ---- 第 4 行：报警优先，否则显示雨情 ----
     * 水位超过阈值时显示“水位过高”，
     * 此时雨情不再占用第 4 行（保证报警信息最显眼）。
     */
    if (master_data.waterAlarm)
    {
        OLED_ShowString(0, 6, "水位过高");
    }
    else
    {
        switch (master_data.rainLevel)
        {
        case 1:
            OLED_ShowString(0, 6, "小雨");
            break;
        case 2:
            OLED_ShowString(0, 6, "中雨");
            break;
        case 3:
            OLED_ShowString(0, 6, "大雨");
            break;
        default:
            OLED_ShowString(0, 6, "无雨");
            break;
        }
    }

    OLED_Refresh();
}

/**
 * @brief 显示"等待数据"画面
 */
void Master_ShowWaiting(void)
{
    OLED_Clear();
    OLED_ShowString(0, 0, "等待从机");
    OLED_ShowString(0, 2, "LoRa 数据...");
    OLED_Refresh();
}

/**
 * @brief 主机初始化：OLED 初始化 + 等待画面
 */
void Master_Init(void)
{
    I2C_GPIO_Init();
    OLED_Init();
    OLED_Clear();
    OLED_Refresh();

    memset(&master_data, 0, sizeof(master_data));

    Master_ShowWaiting();
}
