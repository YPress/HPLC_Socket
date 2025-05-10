#ifndef POWER_STRIP_H
#define POWER_STRIP_H

#include <Arduino.h>
#include <Global.h>
#include <Persistence.h> // 依赖持久化模块
#include <vector>        // 用 vector 存储排插列表

// 定义[插孔信息]
struct Socket
{
    bool state;        // 插孔[通断状态] (true: 通电, false: 断电)
    uint16_t maxPower; // 插孔[最大功率] (W)
};

// 定义[排插信息]
struct PowerStrip
{
    uint8_t macAddress[6]; // 排插[MAC地址] (6字节)
    String name;           // 排插[设备名称]
    Socket sockets[3];     // 排插[插孔信息] (最多3个插孔)
    bool isOnline;         // 排插[是否在线] (true: 在线, false: 离线)
};

/**
 * @brief 初始化排插管理器
 * @details 从持久化存储（NVS）中加载所有已保存的排插数据到内存中
 */
void PowerStrip_init();

/**
 * @brief 添加一个新的排插到管理器中
 * @param strip 要添加的排插对象，包含MAC地址、名称和插孔信息
 * @return bool 添加成功返回 true，如果MAC地址已存在则返回 false
 */
bool PowerStrip_add(const PowerStrip &strip);

/**
 * @brief 根据MAC地址更新现有排插的信息
 * @param strip 包含更新后信息的排插对象，其MAC地址必须与要更新的排插匹配
 * @return bool 更新成功返回 true，如果未找到对应MAC地址的排插则返回 false
 */
bool PowerStrip_update(const PowerStrip &strip);

/**
 * @brief 根据MAC地址删除一个排插
 * @param macAddress 要删除的排插的MAC地址
 * @return bool 删除成功返回 true，如果未找到对应MAC地址的排插则返回 false
 */
bool PowerStrip_delete(const uint8_t macAddress[6]);

/**
 * @brief 根据MAC地址获取指定排插的信息
 * @param macAddress 要查询的排插的MAC地址
 * @param strip 用于接收排插信息的输出参数（引用）
 * @return bool 找到排插并成功获取信息返回 true，否则返回 false
 */
bool PowerStrip_get(const uint8_t macAddress[6], PowerStrip &strip);

/**
 * @brief 获取当前管理器中所有排插的信息
 * @return std::vector<PowerStrip> 包含所有排插对象的向量副本
 */
std::vector<PowerStrip> PowerStrip_get_all();

/**
 * @brief 删除所有排插信息
 * @details 清除持久化存储中的所有排插数据，并清空内存中缓存的排插列表
 */
void PowerStrip_delete_all();

#endif