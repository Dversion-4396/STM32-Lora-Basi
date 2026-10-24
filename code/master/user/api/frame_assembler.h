#ifndef _FRAME_ASSEMBLER_H
#define _FRAME_ASSEMBLER_H

#include "common.h"

/*
================================================================================
【模块说明】LoRa 流式拼帧器
--------------------------------------------------------------------------------
    为什么需要这个模块：
        AS32-TTL-100-C 模块通过 LoRa 空口收到一包数据后，不是一次性
        把整包吐到串口，而是按固定块大小（实测 32 字节）分块输出，
        块与块之间串口会短暂空闲，触发 IDLE 中断。

        从机发出的完整帧是：
            $RAIN,DHT20,T=26.5,H=52.2,ADC=4092,V=3297,WET=0,DO=1,LEVEL=0,WL=80*8B\r\n
        共约 79 字节，会被拆成 32 + 32 + 15 三段到达。

        如果每收到一段就当"一帧"去解析，必然失败。
        所以需要先把碎片拼成完整一行（以 \r\n 结尾），再交给解析器。

    本模块职责：
        1. 接收任意长度的字节流碎片，追加到内部缓冲
        2. 在缓冲中扫描 '\n'（即 "\r\n" 的结尾）作为帧边界
        3. 每凑出完整一行就回调出去，剩下的留在缓冲里等后续碎片
        4. 行首不是 '$' 的垃圾数据自动丢弃（避免上一帧残留干扰）
        5. 缓冲满时自动丢弃最旧的垃圾，防止溢出
================================================================================
*/

/* 拼帧缓冲容量。完整帧约 79 字节，留 2 倍余量 */
#define FRAME_ASM_BUF_SIZE (256)

/**
 * @brief 拼帧器状态结构
 */
typedef struct
{
    uint8_t  buf[FRAME_ASM_BUF_SIZE];   /* 拼帧缓冲 */
    uint16_t len;                       /* 当前已缓存字节数 */
} FrameAssembler_t;

/**
 * @brief 复位拼帧器（清空缓冲）
 * @param fa 拼帧器
 */
void FrameAsm_Reset(FrameAssembler_t *fa);

/**
 * @brief 喂入一段字节流，凑出完整帧就通过回调输出
 *
 * @param fa       拼帧器
 * @param data     新收到的字节（可以是任意长度的碎片）
 * @param len      本次字节数
 * @param out      输出缓冲区，接收拼好的完整一行
 * @param outSize  out 的容量
 * @param outLen   输出参数：拼出的帧长度（不含 '\r\n'）
 * @return 1 = 拼出了一帧完整数据（可调用多次直到返回 0）
 *
 * @note 调用方式：循环调用直到返回 0，把本次喂入的数据里所有完整帧都取完
 */
uint8_t FrameAsm_Push(FrameAssembler_t *fa,
                      const uint8_t *data,
                      uint16_t len,
                      uint8_t *out,
                      uint16_t outSize,
                      uint16_t *outLen);

#endif
