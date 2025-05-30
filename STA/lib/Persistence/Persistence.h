#ifndef PERSISTENCE_H
#define PERSISTENCE_H

#include <Arduino.h>
#include <Preferences.h> // 非易失性存储 (NVS)

/**
 * @brief 初始化持久化存储模块
 * @details 打开或创建指定命名空间 (namespace) 的 Preferences 实例，如果已经初始化了不同的命名空间，会先结束之前的实例
 * @param ns 要使用的命名空间的名称 (例如 "config", "userdata")
 */
void persistence_init(const char *ns);

/**
 * @brief 结束（关闭）当前打开的持久化存储实例
 * @details 释放 Preferences 对象占用的资源通常在不再需要访问 NVS 时调用，或者在切换到不同命名空间之前由 persistence_init 内部调用
 */
void persistence_end();

/**
 * @brief 将字节数据（二进制块）存储到当前命名空间下的指定键
 * @param key 存储数据的键名 (字符串)
 * @param value 指向要存储数据的内存缓冲区的指针
 * @param len 要存储的数据的字节长度
 * @return bool 存储成功返回 true，失败（如未初始化或 NVS 错误）返回 false
 */
bool persistence_put_bytes(const char *key, const void *value, size_t len);

/**
 * @brief 从当前命名空间下的指定键读取字节数据
 * @param key 要读取数据的键名
 * @param buf 指向用于接收数据的内存缓冲区的指针
 * @param maxLen 缓冲区 buf 的最大容量（字节）
 * @return size_t 实际读取到的字节数如果键不存在或发生错误，返回 0
 */
size_t persistence_get_bytes(const char *key, void *buf, size_t maxLen);

/**
 * @brief 将字符串存储到当前命名空间下的指定键
 * @param key 存储字符串的键名
 * @param value 要存储的 Arduino String 对象
 * @return bool 存储成功返回 true，失败返回 false
 */
bool persistence_put_string(const char *key, String value);

/**
 * @brief 从当前命名空间下的指定键读取字符串
 * @param key 要读取字符串的键名
 * @param defaultValue 如果键不存在或读取失败时返回的默认字符串
 * @return String 读取到的字符串，或者 defaultValue
 */
String persistence_get_string(const char *key, const String &defaultValue = "");

/**
 * @brief 从当前命名空间中删除指定的键值对
 * @param key 要删除的键名
 * @return bool 删除成功或键原本就不存在时返回 true，失败返回 false
 */
bool persistence_remove(const char *key);

/**
 * @brief 清除当前命名空间下的所有键值对
 * @details 谨慎使用！这将删除该命名空间中的所有数据
 * @return bool 清除成功返回 true，失败返回 false
 */
bool persistence_clear();

#endif