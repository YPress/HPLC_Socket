#ifndef GLOBAL_H
#define GLOBAL_H

#include <Arduino.h>
#include <sstream>    // 将MAC地址转换为字符串
#include <iomanip>    // 格式化十六进制输出
#include <functional> // lambda函数std::function

// 定义[获取数组长度]函数
#define ARRAY_LENGTH(arr) (sizeof(arr) / sizeof(arr[0]))

// 定义[协议帧常量](参考DL/T645-2007)
#define FRAME_LEAD_BYTE 0xFE // 前导字节
#define FRAME_HEADER 0x68    // 帧起始符
#define FRAME_END 0x16       // 帧结束符
#define MAX_FRAME_LEN 64     // 最大帧长度

// 定义[帧解析状态枚举]类型
typedef enum
{
    WAIT_LEAD_BYTE,   // 前导字节检测
    WAIT_HEADER,      // 第1个起始符检测
    READING_CTRL,     // 控制码读取
    READING_DATA_LEN, // 数据域长度读取
    READING_DATA,     // 数据域读取
    READING_CHECKSUM, // 校验和验证
    WAIT_EOF          // 结束符检查
} FrameState;

// 定义[帧解析器]类型
typedef struct
{
    FrameState state;           // 状态 -> 前导字节检测（初始状态）
    byte buffer[MAX_FRAME_LEN]; // 帧内容缓冲区
    int index;                  // 帧内容缓冲区下标
    int dataFieldEndIndex;      // 数据域结束下标
    byte checksum;              // 校验和
} FrameParser;

// 定义[帧回调函数]类型
typedef std::function<void(FrameParser)> FrameCallbackFunc;

/**
 * @brief 将数据打印到串口监视器
 * @param prefix 前缀字符串
 * @param data 要打印的数据
 */
void print_to_serial_monitor(const char *prefix, uint8_t data);

/**
 * @brief 将6字节的MAC地址转换为十六进制字符串表示
 * @param macAddress 6字节的MAC地址数组
 * @return String MAC地址的十六进制字符串形式 (如 "AABBCCDDEEFF")
 */
String mac_to_string(const uint8_t macAddress[6]);

/**
 * @brief 将12字符的MAC地址字符串转换为6字节的uint8_t数组
 * @param macString 12字符的MAC地址字符串 (如 "AABBCCDDEEFF")
 * @param macAddress 输出参数，用于存储转换后的6字节MAC地址数组
 */
void string_to_mac(const String &macString, uint8_t macAddress[6]);

#endif