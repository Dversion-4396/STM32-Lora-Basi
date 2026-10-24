#include "frame_assembler.h"

/*
================================================================================
【实现说明】LoRa 流式拼帧器
--------------------------------------------------------------------------------
    核心思路：
        把收到的碎片全部追加到 fa->buf，然后在 buf 里找 '\n'。
        找到就说明从 buf 开头到 '\n' 之间是一整行（完整帧），
        把它拷出去，剩下的字节左移回 buf 开头，继续找下一行。

    为什么要"行首必须是 '$'"：
        如果上一次因为某种原因残留了半帧（比如从机中途重启），
        残留的尾部会和新帧的开头粘在一起，形成
            "=0,WL=80*8B\r\n$RAIN,..."
        这种数据解析必然失败，还会把后面正常帧的开头吃掉。
        所以每拼出一行，先检查是否以 '$' 开头，不是就整行丢弃。

    缓冲溢出的处理：
        正常帧约 79 字节，缓冲 256 字节足够放 3 帧。
        如果一直找不到 '\n'（比如收到的全是垃圾），
        缓冲满了就清空重来，绝不让 len 越界。
================================================================================
*/

/* 帧起始符：本工程所有数据帧都以 '$' 开头 */
#define FRAME_START_CHAR ((uint8_t)'$')

/**
 * @brief 复位拼帧器
 */
void FrameAsm_Reset(FrameAssembler_t *fa)
{
    if (fa == NULL)
    {
        return;
    }
    fa->len = 0;
}

/**
 * @brief 在缓冲中查找换行符 '\n'，找到返回下标，找不到返回 -1
 */
static int32_t FindNewline(const uint8_t *buf, uint16_t len)
{
    uint16_t i;

    for (i = 0; i < len; i++)
    {
        if (buf[i] == (uint8_t)'\n')
        {
            return (int32_t)i;
        }
    }

    return -1;
}

/**
 * @brief 喂入一段字节流，凑出完整帧就通过 out 输出
 */
uint8_t FrameAsm_Push(FrameAssembler_t *fa,
                      const uint8_t *data,
                      uint16_t len,
                      uint8_t *out,
                      uint16_t outSize,
                      uint16_t *outLen)
{
    uint16_t copyLen;
    int32_t  nlIdx;
    uint16_t lineLen;      /* 不含 '\r' '\n' 的有效长度 */
    uint16_t consumed;     /* 本次从缓冲头部吃掉的字节数（含换行） */
    uint16_t remain;
    uint16_t i;

    if ((fa == NULL) || (data == NULL) || (out == NULL) || (outLen == NULL))
    {
        return 0;
    }

    /* ---------- 1. 追加新数据到缓冲（放不下就截断） ---------- */
    copyLen = len;
    if ((uint32_t)fa->len + copyLen > FRAME_ASM_BUF_SIZE)
    {
        copyLen = (uint16_t)(FRAME_ASM_BUF_SIZE - fa->len);
    }

    for (i = 0; i < copyLen; i++)
    {
        fa->buf[fa->len + i] = data[i];
    }
    fa->len = (uint16_t)(fa->len + copyLen);

    /*
     * 2. 找换行符。找到就说明凑齐了一整行。
     *    注意：一次 Push 里可能有多行（虽然正常情况一帧一行），
     *    这里只处理最前面那一行，剩下的留着下次调用继续处理。
     */
    nlIdx = FindNewline(fa->buf, fa->len);
    if (nlIdx < 0)
    {
        /*
         * 3. 还没收到换行，继续等后续碎片。
         *    但如果缓冲已经满了还找不到换行，说明收到的是垃圾流，
         *    直接清空，避免把后面的正常帧全部堵死。
         */
        /*
         * 【关键修复】缓冲里一个 '$' 都没有，说明收到的全是垃圾
         * （比如模块上电瞬间的噪声、或上一帧被截断后的尾部）。
         * 这类数据永远等不到换行，会一直堵在缓冲里，
         * 导致后面的正常帧也拼不出来。直接清空，让正常帧能重新同步。
         */
        {
            uint16_t k;
            uint8_t  hasStart = 0;

            for (k = 0; k < fa->len; k++)
            {
                if (fa->buf[k] == FRAME_START_CHAR)
                {
                    hasStart = 1;
                    break;
                }
            }

            if (!hasStart)
            {
                fa->len = 0;
            }
            else if (fa->len >= FRAME_ASM_BUF_SIZE)
            {
                fa->len = 0;
            }
        }
        return 0;
    }

    /* 4. 计算这一行的有效长度（去掉行尾的 '\r' 和 '\n'） */
    lineLen = (uint16_t)nlIdx;
    if ((lineLen > 0) && (fa->buf[lineLen - 1] == (uint8_t)'\r'))
    {
        lineLen--;
    }

    /* 本次从缓冲头部消耗掉的字节数（包含换行符本身） */
    consumed = (uint16_t)(nlIdx + 1);

    /*
     * 5. 行首必须是 '$'，否则是残留垃圾，整行丢弃。
     *    这是关键：能挡住 "=0,WL=80*8B" 这种半截数据。
     */
    if ((lineLen == 0) || (fa->buf[0] != FRAME_START_CHAR))
    {
        /* 丢掉这一行，把剩余字节左移 */
        remain = (uint16_t)(fa->len - consumed);
        for (i = 0; i < remain; i++)
        {
            fa->buf[i] = fa->buf[consumed + i];
        }
        fa->len = remain;
        return 0;
    }

    /* 6. 输出长度不能超过调用方给的缓冲 */
    if (lineLen > outSize)
    {
        lineLen = outSize;
    }

    for (i = 0; i < lineLen; i++)
    {
        out[i] = fa->buf[i];
    }
    *outLen = lineLen;

    /* 7. 把这一行从缓冲里移除，剩余字节左移 */
    remain = (uint16_t)(fa->len - consumed);
    for (i = 0; i < remain; i++)
    {
        fa->buf[i] = fa->buf[consumed + i];
    }
    fa->len = remain;

    return 1;
}
