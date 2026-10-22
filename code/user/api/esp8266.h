#ifndef _ESP8266_H
#define _ESP8266_H

#include "common.h"

#include "usart3.h"
#include "usart1.h"
#include "delay.h"

/**
 * @brief 通过 USART3 发送 AT 指令字符串到 ESP8266 模块
 * 
 * @param at_cmd  当前通过 USART3 模块发送到 ESP8266 的 AT 指令字符串，要求满足
 *                  AT 指令字符串格式要求。
 * @param ack     用于判断当前发送到 ESP8266 WiFi 模块对应 AT 指令是否执行成功，
 *                  当前 ack 对应应答字符串期望，例如 "OK", 特定回显内容字符 "+CWLAP"
 * @param timeout 当前 AT 等待时间
 * @return ESP8266 执行 AT 指令响应情况下成功，返回 0，否则返回 1。
 */
u8 USART3_Send_AT_Cmd_To_ESP8266(const char *at_cmd, const char *ack, u16 timeout);

/**
 * @brief ESP8266 WiFi 模块连接 AP 热点，要求
 *      1. 对应热点必须是 2.4GHz 频段，同时主要 AP 热点可连接数量参数
 *      2. 要求提供对应设备的 SSID 和 PSK，SSID 和 PSK 不可以有中文。
 * 
 * @param ssid 目标连接热点 AP 信息
 * @param psk  对应 AP 热点的密码
 */
void ESP8266_Connect_AP(const char *ssid, const char *psk);

/**
 * @brief 当前 ESP8266 通过 TCP 协议，根据对应的 IP 地址和端口号连接目标 MQTT 云平台服务器
 *    
 * @param protocol 目标网络连接协议，要求是字符串形式 "TCP" or "UDP"
 * @param ip_addr  目标服务器 IP 地址字符串 or 域名字符串
 * @param port     目标 MQTT 服务器提供服务器的端口
 * @return 连接成功，返回 0，否则返回 1
 */
u8 ESP8266_Connect_MQTT_Server(const char *protocol, const char *ip_addr, short port);

/**
 * @brief 当前 ESP8266 模块开启透明传输模式，方便后续 ESP8266 直接将 MQTT 数据包内容
 *      不进行任何处理，直接发送到 MQTT 服务器。
 */
void ESP8266_Open_Transparent_Transmit_Mode(void);

/**
 * @brief 退出透传模式
 */
void ESP8266_Close_Transparent_Transmit_Mode(void);

#endif

