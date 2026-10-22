#include "mqtt.h"

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
                               u16 keep_alive_time)
{
    /*
    1. 定义必要变量
    */
    u16 client_len = strlen(client);
    u16 username_len = strlen(username);
    u16 password_len = strlen(password);
    u16 mqtt_protocol_len = strlen("MQTT");

    /*
    可变头字节数
        1. Protocol Name 2 + N MQTT 字符串形式对应协议名称 "MQTT" 描述，对应 6 个字节
        2. Protocol Level 协议等级 一个字节
        3. Content Flags 连接标志位信息  一个字节
        4. Keep Alive 时间对应 两个字节
    */
    u16 variable_header_len = 10;
    /*
    有效载荷字节数
        1. 2 + N MQTT 字符串形式 客户端名称
        2. 2 + N MQTT 字符串形式 用户名
        3. 2 + N MQTT 字符串形式 密码
    */
    u16 payload_len = 2 + client_len + 2 + username_len + 2 + password_len;

    /*
    2. 准备一个容量为 128 字节的数组，作为 MQTT 数据包构成
    */
    u8 MQTT_Connect_Package[MQTT_PACKAGE_SIZE] = {0};
    u8 pos = 0;
    /*
    3. 【MQTT CONNECT 数据包对应 Fixed Header 固定头内容】
        3.1 CONNECT 数据包 + 数据包类型标志位
        3.2 剩余数据包字节数 => variable_header_len  + payload_len
    */
    MQTT_Connect_Package[pos++] = 0x10; // 0001 0000
    MQTT_Connect_Package[pos++] = variable_header_len + payload_len;

    /*
    4. 【MQTT CONNECT 数据包对应 Variable Header 可变头内容】
        4.1 Protocol Name 2 + N MQTT 字符串形式对应协议名称 "MQTT" 描述，对应 6 个字节
        4.2 Protocol Level 协议等级 一个字节
        4.3 Content Flags 连接标志位信息  一个字节
        4.4 Keep Alive 时间对应 两个字节
    */
    //  4.1 Protocol Name 2 + N MQTT 字符串形式对应协议名称 "MQTT" 描述，对应 6 个字节
    MQTT_Connect_Package[pos++] = mqtt_protocol_len >> 8;
    MQTT_Connect_Package[pos++] = mqtt_protocol_len & (0x00FF);
    memcpy(MQTT_Connect_Package + pos, "MQTT", mqtt_protocol_len);
    pos += mqtt_protocol_len;

    // 4.2 Protocol Level 协议等级 一个字节
    MQTT_Connect_Package[pos++] = 0x04;

    // 4.3 Content Flags 连接标志位信息  一个字节
    MQTT_Connect_Package[pos++] = 0xC2;

    // 4.4 Keep Alive 时间对应 两个字节
    MQTT_Connect_Package[pos++] = keep_alive_time >> 8;
    MQTT_Connect_Package[pos++] = keep_alive_time & (0x00FF);

    /*
    5. 【MQTT CONNECT 数据包对应 Payload 有效载荷内容】
        5.1 2 + N MQTT 字符串形式 客户端名称
        5.2 2 + N MQTT 字符串形式 用户名
        5.3 2 + N MQTT 字符串形式 密码
    */
    // 5.1 2 + N MQTT 字符串形式 客户端名称
    MQTT_Connect_Package[pos++] = client_len >> 8;
    MQTT_Connect_Package[pos++] = client_len & (0x00FF);
    memcpy(MQTT_Connect_Package + pos, client, client_len);
    pos += client_len;

    // 5.2 2 + N MQTT 字符串形式 用户名
    MQTT_Connect_Package[pos++] = username_len >> 8;
    MQTT_Connect_Package[pos++] = username_len & (0x00FF);
    memcpy(MQTT_Connect_Package + pos, username, username_len);
    pos += username_len;

    // 5.3 2 + N MQTT 字符串形式 密码
    MQTT_Connect_Package[pos++] = password_len >> 8;
    MQTT_Connect_Package[pos++] = password_len & (0x00FF);
    memcpy(MQTT_Connect_Package + pos, password, password_len);
    pos += password_len;

    /*
    【延伸思考】
        1. 当前代码是否可以封装函数
        2. 针对于 MQTT 字符串所需的 2 + N 模式是否可以自定义类型使用
    */

	//  USART1_SendBuffer(MQTT_Connect_Package, pos);
    // 6. 将 MQTT Connect 利用 WiFi 模块发送到云平台
    USART3_SendBuffer(MQTT_Connect_Package, pos);
}

/**
 * @brief MCU 发送数据到 MQTT 云平台，要求用户提供
 *      1. 对应 MQTT 云平台发布数据主题字符串
 *      2. 数据内容和对应长度
 *
 * @param publish_topic 对应 MQTT 云平台发布数据主题字符串
 * @param payload        发布数据有效载荷内容，通常为 JSON 格式字符串
 */
void MQTT_Publish_Package_Send(const char *publish_topic,
                               const char *payload)
{
    // 1. 准备必要变量内容
    u16 publish_topic_len = strlen(publish_topic);
    u16 payload_len = strlen(payload);

    // 可变字节数，对应 2 + N 字符串发布主题
    u16 variable_header_len = 2 + publish_topic_len;

    /*
    2. 准备一个容量为 128 字节的数组，作为 MQTT 数据包构成
    */
    u8 MQTT_Publish_Package[MQTT_PACKAGE_SIZE] = {0};
    u8 pos = 0;

    /*
    3. 【MQTT PUBLISH 数据包对应 Fixed Header 固定头内容】
        3.1 PUBLISH 数据包 + 数据包类型标志位
        3.2 剩余数据包字节数 => variable_header_len  + payload_len
    */
    MQTT_Publish_Package[pos++] = 0x30;
    MQTT_Publish_Package[pos++] = variable_header_len + payload_len;

    /*
    4. 【MQTT PUBLISH 数据包对应 Variable Header 可变头内容】
        Publish Topic 发布主题 2 + N MQTT 字符串形式
    */
    MQTT_Publish_Package[pos++] = publish_topic_len >> 8;
    MQTT_Publish_Package[pos++] = publish_topic_len & 0x00FF;
    memcpy(MQTT_Publish_Package + pos, publish_topic, publish_topic_len);
    pos += publish_topic_len;

    /*
    5. 【MQTT PUBLISH 数据包对应 Payload 有效载荷内容】
        对应用户提供的 JSON 格式字符串内容
    */
    memcpy(MQTT_Publish_Package + pos, payload, payload_len);
    pos += payload_len;

    // USART1_SendBuffer(MQTT_Publish_Package, pos);
    // 6. 将 MQTT PUBLISH 利用 WiFi 模块发送到云平台
    USART3_SendBuffer(MQTT_Publish_Package, pos);
}

/**
 * @brief MCU 发送订阅主题数据包到 MQTT 云平台，可以收到云平台下发的数据
 *
 * @param subscribe_topic 订阅主题字符串
 */
void MQTT_Subscribe_Package_Send(const char *subscribe_topic)
{
    // 1. 准备必要变量内容
    u16 subscribe_topic_len = strlen(subscribe_topic);

    /*
    2. 准备一个容量为 128 字节的数组，作为 MQTT 数据包构成
    */
    u8 MQTT_Subscribe_Package[MQTT_PACKAGE_SIZE] = {0};
    u8 pos = 0;

    /*
    3. 【MQTT SUBSCRIBE  数据包对应 Fixed Header 固定头内容】
        3.1 SUBSCRIBE 数据包 + 数据包类型标志位
            1000 0010

        3.2 剩余数据包字节数 => 2 + 2 + subscribe_topic_len + 1
    */
    MQTT_Subscribe_Package[pos++] = 0x82;
    MQTT_Subscribe_Package[pos++] = 2 + 2 + subscribe_topic_len + 1;

    /*
    4. 【MQTT SUBSCRIBE 数据包对应 Variable Header 可变头内容】
    */
    MQTT_Subscribe_Package[pos++] = 10 >> 8;
    MQTT_Subscribe_Package[pos++] = 10 & 0x00FF;

    /*
    5. 【MQTT SUBSCRIBE 数据包对应 Payload 有效载荷内容】
        5.1 2 + N MQTT 字符串格式对应的订阅主题字符串
        5.2 1 字节 QoS 服务等级
    */
    // 5.1 2 + N MQTT 字符串格式对应的订阅主题字符串
    MQTT_Subscribe_Package[pos++] = subscribe_topic_len >> 8;
    MQTT_Subscribe_Package[pos++] = subscribe_topic_len & 0x00FF;
    memcpy(MQTT_Subscribe_Package + pos, subscribe_topic, subscribe_topic_len);
    pos += subscribe_topic_len;

    // 5.2 1 字节 QoS 服务等级
    MQTT_Subscribe_Package[pos++] = 0;

    // USART1_SendBuffer(MQTT_Subscribe_Package, pos);
    // 6. 将 MQTT Connect 利用 WiFi 模块发送到云平台
    USART3_SendBuffer(MQTT_Subscribe_Package, pos);
}
