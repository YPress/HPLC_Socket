#include <PowerStrip.h> // 包含排插管理器头文件

// 内存中的[排插列表]，存储所有已知的排插信息
static std::vector<PowerStrip> powerStrips;
// 持久化存储使用的[命名空间]，用于隔离排插数据
static const char *nvsNamespace = "powerstrips";
// 存储排插MAC地址列表的[键名]
static const char *indexKey = "index";

/**
 * @brief 从持久化存储（NVS）加载所有排插数据到内存中的 powerStrips 向量
 * @details 在 PowerStrip_init() 中调用
 */
static void load_from_persistence()
{
    // 清空当前内存中的排插列表，准备重新加载
    powerStrips.clear();
    // 初始化或打开指定命名空间的持久化存储
    persistence_init(nvsNamespace);

    // 读取存储在 indexKey 下的 MAC 地址索引字符串
    String indexStr = persistence_get_string(indexKey, ""); // 如果键不存在，返回空字符串
    // 检查索引是否为空
    if (indexStr.length() > 0)
    {
        // 解析索引字符串，格式为 "mac1,mac2,mac3,..."
        int start = 0;                        // 当前处理的[MAC 地址字符串]的起始位置
        int commaPos = indexStr.indexOf(','); // 查找下一个逗号的位置

        // 循环处理逗号分隔的[MAC 地址字符串]
        while (commaPos != -1)
        {
            // 提取[MAC 地址字符串]
            String macStr = indexStr.substring(start, commaPos);
            // 创建新的 PowerStrip 对象来存储数据
            PowerStrip strip;
            // 根据[MAC 地址字符串]构造用于存储数据的键名
            String dataKey = macStr;           // [原始 MAC 地址(为确保一致性)]键名
            String nameKey = macStr + "_n";    // [排插设备名称]键名
            String socketsKey = macStr + "_s"; // [插孔信息]键名

            // 从持久化存储中读取排插的具体数据
            persistence_get_bytes(dataKey.c_str(), strip.macAddress, 6);                     // 读取[MAC 地址]
            strip.name = persistence_get_string(nameKey.c_str(), "");                        // 读取[排插设备名称]
            persistence_get_bytes(socketsKey.c_str(), strip.sockets, sizeof(strip.sockets)); // 读取[插孔信息]
            strip.isOnline = false;                                                          // 初始化[在线状态]为离线

            // 将加载的排插添加到内存列表中
            powerStrips.push_back(strip);

            // 更新下一次查找的起始位置
            start = commaPos + 1;
            // 查找下一个逗号
            commaPos = indexStr.indexOf(',', start);
        }
        // 处理索引字符串中的最后一个 MAC 地址 (没有后续逗号)
        String macStr = indexStr.substring(start);
        // 确保最后一部分不是空的
        if (macStr.length() > 0)
        {
            PowerStrip strip;
            String dataKey = macStr;
            String nameKey = macStr + "_n";
            String socketsKey = macStr + "_s";

            // 从持久化存储中读取排插的具体数据
            persistence_get_bytes(dataKey.c_str(), strip.macAddress, 6);
            strip.name = persistence_get_string(nameKey.c_str(), "");
            persistence_get_bytes(socketsKey.c_str(), strip.sockets, sizeof(strip.sockets));
            strip.isOnline = false;

            // 将加载的排插添加到内存列表中
            powerStrips.push_back(strip);
        }
    }

    // persistence_end();
}

/**
 * @brief 将单个排插的数据保存到持久化存储（NVS）
 * @param strip 要保存的排插对象
 * @return bool 保存成功返回 true，失败返回 false
 */
static bool save_to_persistence(const PowerStrip &strip)
{
    // 确保持久化存储已初始化
    persistence_init(nvsNamespace);
    // 获取[MAC 地址字符串]
    String macStr = mac_to_string(strip.macAddress);
    // 构造用于存储数据的键名
    String dataKey = macStr;
    String nameKey = macStr + "_n";
    String socketsKey = macStr + "_s";

    // 标记保存操作是否成功
    bool success = true;
    // 将排插的各个字段分别存入持久化存储
    success &= persistence_put_bytes(dataKey.c_str(), strip.macAddress, 6);                     // 存储 MAC 地址字节
    success &= persistence_put_string(nameKey.c_str(), strip.name);                             // 存储名称
    success &= persistence_put_bytes(socketsKey.c_str(), strip.sockets, sizeof(strip.sockets)); // 存储[插孔信息]

    // persistence_end();
    return success;
}

/**
 * @brief 从持久化存储（NVS）中删除指定 MAC 地址的排插数据
 * @param macAddress 要删除的排插的 MAC 地址
 * @return bool 删除成功返回 true，失败返回 false
 */
static bool remove_from_persistence(const uint8_t macAddress[6])
{
    // 确保持久化存储已初始化
    persistence_init(nvsNamespace);
    // 获取[MAC 地址字符串]
    String macStr = mac_to_string(macAddress);
    // 构造要删除的键名
    String dataKey = macStr;
    String nameKey = macStr + "_n";
    String socketsKey = macStr + "_s";

    // 标记删除操作是否成功
    bool success = true;
    // 从持久化存储中移除与该排插相关的键值对
    success &= persistence_remove(dataKey.c_str());
    success &= persistence_remove(nameKey.c_str());
    success &= persistence_remove(socketsKey.c_str());

    // persistence_end();
    return success;
}

/**
 * @brief 更新持久化存储中的索引字符串（排插 MAC 地址列表）
 * @details 此函数在添加或删除排插后调用，以保持索引与内存列表同步
 */
static void update_persistence_index()
{
    // 确保持久化存储已初始化
    persistence_init(nvsNamespace);
    // 初始化空的索引字符串
    String indexStr = "";
    // 遍历内存中的所有排插
    for (size_t i = 0; i < powerStrips.size(); ++i)
    {
        // 将每个排插的[MAC 地址字符串]添加到索引中
        indexStr += mac_to_string(powerStrips[i].macAddress);
        // 如果不是最后一个排插，则添加逗号分隔符
        if (i < powerStrips.size() - 1)
        {
            indexStr += ",";
        }
    }
    // 将更新后的索引字符串写回持久化存储
    persistence_put_string(indexKey, indexStr);

    // persistence_end();
}

/**
 * @brief 初始化排插管理器
 * @details 从持久化存储（NVS）中加载所有已保存的排插数据到内存中
 */
void PowerStrip_init()
{
    // 调用内部函数从持久化存储加载数据
    load_from_persistence();
}

/**
 * @brief 添加一个新的排插到管理器中
 * @param strip 要添加的排插对象，包含MAC地址、名称和插孔信息
 * @return bool 添加成功返回 true，如果MAC地址已存在则返回 false
 */
bool PowerStrip_add(const PowerStrip &strip)
{
    // 检查内存中是否已存在具有相同 MAC 地址的排插
    for (const auto &existingStrip : powerStrips)
    {
        // 比较 MAC 地址
        if (memcmp(existingStrip.macAddress, strip.macAddress, 6) == 0)
        {
            // MAC 地址冲突，添加失败
            return false;
        }
    }

    // 将新的排插添加到内存列表的末尾
    powerStrips.push_back(strip);

    // 尝试将新的排插数据保存到持久化存储
    if (save_to_persistence(strip))
    {
        // 持久化成功，更新持久化存储中的 MAC 地址索引
        update_persistence_index();
        return true;
    }
    else
    {
        // 持久化失败，回滚操作：从内存列表中移除刚刚添加的排插
        powerStrips.pop_back();
        return false;
    }
}

/**
 * @brief 根据MAC地址更新现有排插的信息
 * @param strip 包含更新后信息的排插对象，其MAC地址必须与要更新的排插匹配
 * @return bool 更新成功返回 true，如果未找到对应MAC地址的排插则返回 false
 */
bool PowerStrip_update(const PowerStrip &strip)
{
    // 遍历内存中的排插列表，查找匹配的 MAC 地址
    for (size_t i = 0; i < powerStrips.size(); ++i)
    {
        if (memcmp(powerStrips[i].macAddress, strip.macAddress, 6) == 0)
        {
            // 找到匹配的排插，用新的数据覆盖内存中的旧数据
            powerStrips[i] = strip;
            // 尝试将更新后的数据保存到持久化存储
            if (save_to_persistence(strip))
            {
                // 持久化成功
                // MAC 地址未变，索引不需要更新
                return true;
            }
            else
            {
                // 持久化失败
                // 此时内存数据已更新，但持久化数据未更新，可能导致不一致
                // TODO 考虑实现更复杂的事务或回滚机制
                return false;
            }
        }
    }
    // 遍历完列表未找到匹配的 MAC 地址
    return false;
}

/**
 * @brief 根据MAC地址删除一个排插
 * @param macAddress 要删除的排插的MAC地址
 * @return bool 删除成功返回 true，如果未找到对应MAC地址的排插则返回 false
 */
bool PowerStrip_delete(const uint8_t macAddress[6])
{
    // 使用迭代器遍历内存中的排插列表
    for (auto it = powerStrips.begin(); it != powerStrips.end(); ++it)
    {
        // 检查当前排插的 MAC 地址是否匹配
        if (memcmp(it->macAddress, macAddress, 6) == 0)
        {
            // 找到匹配的排插，尝试从持久化存储中删除其数据
            if (remove_from_persistence(macAddress))
            {
                // 持久化删除成功
                // 从内存列表中移除该排插
                powerStrips.erase(it);
                // 更新持久化存储中的 MAC 地址索引
                update_persistence_index();
                return true;
            }
            else
            {
                // 持久化删除失败
                // 此时内存数据未删除，持久化删除也失败，状态保持不变
                return false;
            }
        }
    }
    // 遍历完列表未找到匹配的 MAC 地址
    return false;
}

/**
 * @brief 根据MAC地址获取指定排插的信息
 * @param macAddress 要查询的排插的MAC地址
 * @param strip 用于接收排插信息的输出参数（引用）
 * @return bool 找到排插并成功获取信息返回 true，否则返回 false
 */
bool PowerStrip_get(const uint8_t macAddress[6], PowerStrip &strip)
{
    // 遍历内存中的排插列表
    for (const auto &existingStrip : powerStrips)
    {
        // 检查 MAC 地址是否匹配
        if (memcmp(existingStrip.macAddress, macAddress, 6) == 0)
        {
            // 找到匹配项，将内存中的数据复制到输出参数 strip 中
            strip = existingStrip;
            return true;
        }
    }
    // 未找到匹配的排插
    return false;
}

/**
 * @brief 获取当前管理器中所有排插的信息
 * @return std::vector<PowerStrip> 包含所有排插对象的向量副本
 */
std::vector<PowerStrip> PowerStrip_get_all()
{
    // 直接返回内存中 powerStrips 向量的副本
    // 返回副本可以防止外部代码直接修改内部状态，但会产生拷贝开销
    return powerStrips;
}

/**
 * @brief 删除所有排插信息
 * @details 清除持久化存储中的所有排插数据，并清空内存中缓存的排插列表
 */
void PowerStrip_delete_all()
{
    // 确保持久化存储已初始化到正确的命名空间
    persistence_init(nvsNamespace);
    // 清除该命名空间下的所有数据 (包括所有排插数据和索引)
    persistence_clear();
    // 不需要单独删除 indexKey，因为它也属于 nvsNamespace

    // 清空内存中的排插列表
    powerStrips.clear();

    // persistence_end();
}