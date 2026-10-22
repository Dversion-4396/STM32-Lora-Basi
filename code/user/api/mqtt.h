#ifndef _MQTT_H
#define _MQTT_H

#include "common.h"
#include "usart3.h"

#define MQTT_PACKAGE_SIZE (128)

/**
 * @brief 请求连接到 MQTT 云平台，要求用户提供的参数包括
 *      1. 当前 MCU 对应客户端名称
 *      2. MQTT 云平台项目实例对应用户名
 *      3. MQTT 云平台项目实例对应密码
 *      4.  Keep Alive 时间
 *
 * @param client          当前 MCU 对应程序客户端名称
 * @param username        MQTT 云平台项目实例对应用户名
 * @param password        MQTT 云平台项目实例对应密码
 * @param keep_alive_time 对应当前 Keep Alive 时间
 */
void MQTT_Connect_Package_Send(const char *client,
                               const char *username,
                               const char *password,
                               u16 keep_alive_time);

/**
 * @brief MCU 发送数据到 MQTT 云平台，要求用户提供
 *      1. 对应 MQTT 云平台发布数据主题字符串
 *      2. 数据内容和对应长度
 *
 * @param publish_topic 对应 MQTT 云平台发布数据主题字符串
 * @param payload        发布数据有效载荷内容，通常为 JSON 格式字符串
 */
void MQTT_Publish_Package_Send(const char *publish_topic,
                               const char *payload);

/**
 * @brief MCU 发送订阅主题数据包到 MQTT 云平台，可以收到云平台下发的数据
 *
 * @param subscribe_topic 订阅主题字符串
 */
void MQTT_Subscribe_Package_Send(const char *subscribe_topic);

#endif
