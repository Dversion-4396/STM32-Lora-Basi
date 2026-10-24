#ifndef _OLED_H
#define _OLED_H

#include "common.h"
#include "i2c.h"
#include "oled_font.h"

// ============================================================
// SSD1306 命令定义（0.96 寸 128x64，4 针 I2C）
// 帧格式：START -> 0x78 -> 0x00(命令) -> 命令 -> STOP
// 带参数的命令分两次发送，先命令后参数
// ============================================================

// ---------- 控制字节 ----------

// 后面跟的是命令
#define OLED_CTRL_CMD 0x00

// 后面跟的是显存数据
#define OLED_CTRL_DATA 0x40

// ---------- 屏幕几何 ----------

// 屏幕列数
#define OLED_WIDTH 128

// 屏幕行数
#define OLED_HEIGHT 64

// 显存页数（64 / 8）
#define OLED_PAGE_COUNT 8

// 显存总字节数（128 x 8 = 1024）
#define OLED_GRAM_SIZE (OLED_WIDTH * OLED_PAGE_COUNT)

// ---------- 显示开关 ----------

// 关显示，初始化第一条
#define OLED_CMD_DISPLAY_OFF 0xAE

// 开显示，初始化最后一条
#define OLED_CMD_DISPLAY_ON 0xAF

// 显示内容跟随显存
#define OLED_CMD_FOLLOW_RAM 0xA4

// 强制全屏点亮，忽略显存，调试用
#define OLED_CMD_ENTIRE_ON 0xA5

// 正常显示（1 亮）
#define OLED_CMD_NORMAL 0xA6

// 反色显示（1 灭）
#define OLED_CMD_INVERSE 0xA7

// ---------- 寻址模式 ----------

// 设置寻址模式（带参数）
#define OLED_CMD_SET_MEM_MODE 0x20

// 水平寻址，可一次连刷整屏
#define OLED_ARG_MEM_MODE_HORIZ 0x00

// 垂直寻址
#define OLED_ARG_MEM_MODE_VERT 0x01

// 页寻址（上电默认）
#define OLED_ARG_MEM_MODE_PAGE 0x02

// 设置列地址范围（带 2 个参数）
#define OLED_CMD_SET_COL_ADDR 0x21

// 设置页地址范围（带 2 个参数）
#define OLED_CMD_SET_PAGE_ADDR 0x22

// 列起始值
#define OLED_COL_START 0x00

// 列结束值
#define OLED_COL_END 0x7F

// 页起始值
#define OLED_PAGE_START 0x00

// 页结束值
#define OLED_PAGE_END 0x07

// 页寻址专用：设置页地址（0xB0 | 页号）
#define OLED_CMD_SET_PAGE_BASE 0xB0

// 页寻址专用：设置列地址低 4 位
#define OLED_CMD_SET_COL_LOW 0x00

// 页寻址专用：设置列地址高 4 位
#define OLED_CMD_SET_COL_HIGH 0x10

// ---------- 时钟与振荡 ----------

// 设置时钟分频与振荡频率（带参数）
#define OLED_CMD_SET_CLK_DIV 0xD5

// 时钟参数默认值
#define OLED_ARG_CLK_DIV_DEFAULT 0x80

// ---------- 复用率 ----------

// 设置复用率（带参数，值为行数减 1）
#define OLED_CMD_SET_MULTIPLEX 0xA8

// 复用率 64 行，128x64 屏用
#define OLED_ARG_MULTIPLEX_64 0x3F

// 复用率 32 行，128x32 屏用
#define OLED_ARG_MULTIPLEX_32 0x1F

// ---------- 显示偏移与起始行 ----------

// 设置显示垂直偏移（带参数）
#define OLED_CMD_SET_DISPLAY_OFFSET 0xD3

// 偏移 0
#define OLED_ARG_OFFSET_0 0x00

// 设置显存起始行（0x40 | 行号）
#define OLED_CMD_SET_START_LINE 0x40

// 起始行 0
#define OLED_ARG_START_LINE_0 0x40

// ---------- 电荷泵 ----------

// 电荷泵开关（带参数）
#define OLED_CMD_CHARGE_PUMP 0x8D

// 电荷泵开，漏发则屏幕全黑
#define OLED_ARG_CHARGE_PUMP_ON 0x14

// 电荷泵关
#define OLED_ARG_CHARGE_PUMP_OFF 0x10

// ---------- 扫描方向 ----------

// 段重映射开，内容左右镜像
#define OLED_CMD_SEG_REMAP_ON 0xA1

// 段重映射关，内容左右正常
#define OLED_CMD_SEG_REMAP_OFF 0xA0

// COM 反向扫描，内容上下颠倒
#define OLED_CMD_COM_SCAN_REVERSE 0xC8

// COM 正常扫描，内容上下正常
#define OLED_CMD_COM_SCAN_NORMAL 0xC0

// ---------- COM 引脚配置 ----------

// 设置 COM 引脚配置（带参数）
#define OLED_CMD_SET_COM_PINS 0xDA

// COM 引脚参数，128x64 屏用
#define OLED_ARG_COM_PINS_128X64 0x12

// COM 引脚参数，128x32 屏用
#define OLED_ARG_COM_PINS_128X32 0x02

// ---------- 对比度 ----------

// 设置对比度（带参数）
#define OLED_CMD_SET_CONTRAST 0x81

// 对比度默认值
#define OLED_ARG_CONTRAST_DEFAULT 0xCF

// 对比度最亮
#define OLED_ARG_CONTRAST_MAX 0xFF

// 对比度最暗
#define OLED_ARG_CONTRAST_MIN 0x00

// ---------- 预充电 ----------

// 设置预充电周期（带参数）
#define OLED_CMD_SET_PRECHARGE 0xD9

// 预充电参数默认值
#define OLED_ARG_PRECHARGE_DEFAULT 0xF1

// ---------- VCOMH 电压 ----------

// 设置 VCOMH 电压（带参数）
#define OLED_CMD_SET_VCOMH 0xDB

// VCOMH 默认值
#define OLED_ARG_VCOMH_DEFAULT 0x40

// VCOMH 约 0.65 VCC
#define OLED_ARG_VCOMH_0_65 0x00

// VCOMH 约 0.77 VCC
#define OLED_ARG_VCOMH_0_77 0x20

// VCOMH 约 0.83 VCC
#define OLED_ARG_VCOMH_0_83 0x30

// ---------- 滚动 ----------

// 停止滚动，初始化建议发一次
#define OLED_CMD_SCROLL_STOP 0x2E

// 开始滚动
#define OLED_CMD_SCROLL_START 0x2F

// 水平向右滚动（带 6 个参数）
#define OLED_CMD_SCROLL_H_RIGHT 0x26

// 水平向左滚动（带 6 个参数）
#define OLED_CMD_SCROLL_H_LEFT 0x27

// 垂直水平向右滚动（带 5 个参数）
#define OLED_CMD_SCROLL_VH_RIGHT 0x29

// 垂直水平向左滚动（带 5 个参数）
#define OLED_CMD_SCROLL_VH_LEFT 0x2A

// ---------- 其他 ----------

// 空操作
#define OLED_CMD_NOP 0xE3

// ---------- 完整初始化序列 ----------

// 初始化序列共 26 字节，命令与参数按发送顺序排列，顺序不可打乱
#define     OLED_INIT_SEQ                                       \
            OLED_CMD_DISPLAY_OFF,                               \
            OLED_CMD_SET_CLK_DIV, OLED_ARG_CLK_DIV_DEFAULT,     \
            OLED_CMD_SET_MULTIPLEX, OLED_ARG_MULTIPLEX_64,      \
            OLED_CMD_SET_DISPLAY_OFFSET, OLED_ARG_OFFSET_0,     \
            OLED_ARG_START_LINE_0,                              \
            OLED_CMD_CHARGE_PUMP, OLED_ARG_CHARGE_PUMP_ON,      \
            OLED_CMD_SET_MEM_MODE, OLED_ARG_MEM_MODE_HORIZ,     \
            OLED_CMD_SEG_REMAP_ON,                              \
            OLED_CMD_COM_SCAN_REVERSE,                          \
            OLED_CMD_SET_COM_PINS, OLED_ARG_COM_PINS_128X64,    \
            OLED_CMD_SET_CONTRAST, OLED_ARG_CONTRAST_DEFAULT,   \
            OLED_CMD_SET_PRECHARGE, OLED_ARG_PRECHARGE_DEFAULT, \
            OLED_CMD_SET_VCOMH, OLED_ARG_VCOMH_DEFAULT,         \
            OLED_CMD_FOLLOW_RAM,                                \
            OLED_CMD_NORMAL,                                    \
            OLED_CMD_SCROLL_STOP,                               \
            OLED_CMD_DISPLAY_ON

// ============================================================
// 显存与函数声明
// ============================================================

// 显存缓冲区，定义在 oled.c
extern u8 OLED_GRAM[8][128];

/// @brief 初始化 OLED 屏幕
void OLED_Init(void);

/// @brief 发送一条命令给 SSD1306
///
/// @param Cmd        要发送的命令字节，如 0xAE 关显示、0xAF 开显示
/// @return 0 表示成功，1 表示失败（重试 I2C_RETRY_COUNT 次后仍失败）
u8 OLED_WriteCmd(u8 Cmd);
/// @brief 向显存写入一个字节数据
///
/// @param data       要写入的 1 字节显存数据，对应一列 8 个像素
/// @return 0 表示成功，1 表示失败
u8 OLED_WriteData(u8 data);
/// @brief 连续写入多个字节显存数据，显存列地址自动递增
///
/// @param buf        要写入的数据缓冲区首地址
/// @param len        要写入的字节数，整屏为 1024
/// @return 0 表示成功，1 表示失败
u8 OLED_WriteDataBuf(const u8 *buf, u16 len);

/// @brief 清空显存缓冲区，只改内存，屏幕要调用 OLED_Refresh 才会变化
void OLED_Clear(void);
/// @brief 设置一个像素点的亮灭
///
/// @param x          列坐标，范围 0 ~ 127
/// @param y          行坐标，范围 0 ~ 63
/// @param color      1 表示点亮，0 表示熄灭
void OLED_SetPixel(u8 x, u8 y, u8 color);
/// @brief 把显存缓冲区整屏发送到屏幕显示，调用后屏幕才会更新
void OLED_Refresh(void);

/// @brief 在指定位置显示字符串，支持 ASCII 和 GBK 汉字混排
///
/// @param x          起始列坐标，范围 0 ~ 127
/// @param page       起始页号，必须是偶数 0/2/4/6，每个字符占 2 页高
/// @param str        要显示的字符串，必须是 GBK 编码
void OLED_ShowString(u8 x, u8 page, const char *str);

// ---------------- GIF 动画播放 ----------------

/// @brief 显示一帧 128x64 全屏位图
///
/// @param bmp        指向 1024 字节位图数据的指针，格式为行优先、bit7 在左、bit=1 点亮
void OLED_DrawBitmap(const u8 *bmp);

/// @brief 播放 GIF 动画，阻塞式，函数内部死循环一直播放不会返回
///
/// @param frames     帧指针表，每个元素指向一帧 1024 字节的位图数据，传 gif_frames
/// @param frame_cnt  总帧数，传 GIF_FRAME_COUNT 即可
/// @param frame_ms   每帧绘制完成后额外延时多少毫秒，越小播放越快
void OLED_PlayGif(const u8 *const *frames, u16 frame_cnt, u16 frame_ms);

/// @brief 播放 GIF 动画的单步版本，非阻塞，每次调用只推进一帧
///
/// @param frames     帧指针表，每个元素指向一帧 1024 字节的位图数据，传 gif_frames
/// @param frame_cnt  总帧数，传 GIF_FRAME_COUNT 即可
/// @param frame      指向当前帧计数器的指针，函数内部会自动加 1 并循环回绕
void OLED_PlayGifStep(const u8 *const *frames, u16 frame_cnt, u16 *frame);

/// @brief 播放 GIF 动画一遍，阻塞式，播完返回继续执行后面的代码
///
/// @param frames     帧指针表，每个元素指向一帧 1024 字节的位图数据，传 gif_frames
/// @param frame_cnt  总帧数，传 GIF_FRAME_COUNT 即可
/// @param frame_ms   每帧绘制完成后额外延时多少毫秒
void OLED_PlayGifOnce(const u8 *const *frames, u16 frame_cnt, u16 frame_ms);

#endif
