#include <Global.h>

/**
 * @brief 将数据打印到串口监视器
 * @param prefix 前缀字符串
 * @param data 要打印的数据
 */
void print_to_serial_monitor(const char *prefix, uint8_t data)
{
    Serial.printf("%s: %02X <%03d> <%c>\n", prefix, data, data, char(data));
}

/**
 * @brief 将6字节的MAC地址转换为十六进制字符串表示
 * @param macAddress 6字节的MAC地址数组
 * @return String MAC地址的十六进制字符串形式 (如 "AABBCCDDEEFF")
 */
String mac_to_string(const uint8_t macAddress[6])
{
    // 创建字符串流
    std::stringstream ss;
    for (int i = 0; i < 6; ++i)
    {
        // 将每个字节转换为2位十六进制数，不足两位前面补0
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)macAddress[i];
    }
    // 返回转换后的 Arduino String 对象
    return String(ss.str().c_str());
}

/**
 * @brief 将12字符的MAC地址字符串转换为6字节的uint8_t数组
 * @param macString 12字符的MAC地址字符串 (如 "AABBCCDDEEFF")
 * @param macAddress 输出参数，用于存储转换后的6字节MAC地址数组
 */
void string_to_mac(const String &macString, uint8_t macAddress[6])
{
    for (int i = 0; i < 6; ++i)
    {
        // 取出两个字符组成一个字节的十六进制表示
        char hex_pair[3] = {macString.charAt(i * 2), macString.charAt(i * 2 + 1), '\0'};
        // 将十六进制字符串转换为字节
        macAddress[i] = strtol(hex_pair, NULL, 16);
    }
}