#include <ElectricRelay.h>
#include <Persistence.h>

// 持久化存储 继电器状态[键名]
#define RELAY1_STATE_KEY "r1_state"
#define RELAY2_STATE_KEY "r2_state"
#define RELAY3_STATE_KEY "r3_state"

// 持久化存储 继电器最大功率[键名]
#define RELAY1_MAX_POWER_KEY "r1_max_power"
#define RELAY2_MAX_POWER_KEY "r2_max_power"
#define RELAY3_MAX_POWER_KEY "r3_max_power"

// 继电器持久化[命名空间]
#define RELAY_PERSISTENCE_NS "RelayStates"

// 存储继电器引脚的数组，方便通过索引访问
static const uint8_t RELAY_PINS[] = {ELECTRIC_RELAY_PIN_1, ELECTRIC_RELAY_PIN_2, ELECTRIC_RELAY_PIN_3};
// 存储继电器当前状态的数组
static uint8_t relay_states[3] = {0, 0, 0}; // 默认断开
// 存储继电器最大功率的数组 (W)
static uint16_t relay_max_powers[3] = {0, 0, 0}; // 默认0W

/**
 * @brief 根据继电器编号和状态实际控制继电器IO口
 * @param relay_index 继电器索引 (0, 1, 或 2)
 * @param state 状态 (0 或 1)
 */
static void _set_relay_pin_state(uint8_t relay_index, uint8_t state)
{
    // 修改IO口输出
    digitalWrite(RELAY_PINS[relay_index], state == 1 ? HIGH : LOW);
    // 更新内存中的状态
    relay_states[relay_index] = state;
}

/**
 * @brief 初始化继电器控制
 * @details 初始化所有继电器的引脚，尝试从持久化存储中加载每个继电器的状态，并将其应用到对应的引脚
 */
void ELECTRIC_RELAY_init()
{
    // 初始化继电器引脚为输出模式
    pinMode(ELECTRIC_RELAY_PIN_1, OUTPUT);
    pinMode(ELECTRIC_RELAY_PIN_2, OUTPUT);
    pinMode(ELECTRIC_RELAY_PIN_3, OUTPUT);

    // 确保持久化模块已初始化
    persistence_init(RELAY_PERSISTENCE_NS);

    uint8_t loaded_state;
    // 加载继电器1的状态
    if (persistence_get_bytes(RELAY1_STATE_KEY, &loaded_state, sizeof(loaded_state)) > 0)
    {
        _set_relay_pin_state(0, loaded_state);
    }
    else
    {
        _set_relay_pin_state(0, 0); // 默认断开
    }

    // 加载继电器2的状态
    if (persistence_get_bytes(RELAY2_STATE_KEY, &loaded_state, sizeof(loaded_state)) > 0)
    {
        _set_relay_pin_state(1, loaded_state);
    }
    else
    {
        _set_relay_pin_state(1, 0); // 默认断开
    }

    // 加载继电器3的状态
    if (persistence_get_bytes(RELAY3_STATE_KEY, &loaded_state, sizeof(loaded_state)) > 0)
    {
        _set_relay_pin_state(2, loaded_state);
    }
    else
    {
        _set_relay_pin_state(2, 0); // 默认断开
    }

    uint16_t loaded_max_power;
    // 加载继电器1的最大功率
    if (persistence_get_bytes(RELAY1_MAX_POWER_KEY, &loaded_max_power, sizeof(loaded_max_power)) > 0)
    {
        relay_max_powers[0] = loaded_max_power;
    }
    else
    {
        relay_max_powers[0] = 0; // 默认0W
    }

    // 加载继电器2的最大功率
    if (persistence_get_bytes(RELAY2_MAX_POWER_KEY, &loaded_max_power, sizeof(loaded_max_power)) > 0)
    {
        relay_max_powers[1] = loaded_max_power;
    }
    else
    {
        relay_max_powers[1] = 0; // 默认0W
    }

    // 加载继电器3的最大功率
    if (persistence_get_bytes(RELAY3_MAX_POWER_KEY, &loaded_max_power, sizeof(loaded_max_power)) > 0)
    {
        relay_max_powers[2] = loaded_max_power;
    }
    else
    {
        relay_max_powers[2] = 0; // 默认0W
    }

    // persistence_end();
}

/**
 * @brief 设置指定继电器的状态
 * @param relay_num 要控制的继电器编号 (1, 2, 3)
 * @param state 要设置的状态 (0 断开 - 低电平, 1 吸合 - 高电平)
 */
void ELECTRIC_RELAY_control(uint8_t relay_num, uint8_t state)
{
    // 确保持久化模块已初始化
    persistence_init(RELAY_PERSISTENCE_NS);

    // 设置继电器状态
    uint8_t relay_index = relay_num - 1;
    _set_relay_pin_state(relay_index, state);

    // 持久化
    const char *key = nullptr;
    switch (relay_num)
    {
    case 1:
        key = RELAY1_STATE_KEY;
        break;
    case 2:
        key = RELAY2_STATE_KEY;
        break;
    case 3:
        key = RELAY3_STATE_KEY;
        break;
    }
    if (key)
    {
        persistence_put_bytes(key, &state, sizeof(state));
    }

    // persistence_end();
}

/**
 * @brief 获取指定继电器的状态
 * @param relay_num 要查询的继电器编号 (1, 2, 3)
 * @return uint8_t 继电器的当前状态 (0 或 1)
 */
uint8_t ELECTRIC_RELAY_get_state(uint8_t relay_num)
{
    // 直接返回内部存储的状态
    return relay_states[relay_num - 1];
    // return digitalRead(RELAY_PINS[relay_num - 1]) == HIGH ? 1 : 0;
}

/**
 * @brief 设置指定继电器的最大功率
 * @param relay_num 要设置的继电器编号 (1, 2, 3)
 * @param power 要设置的最大功率值 (W)
 */
void ELECTRIC_RELAY_set_max_power(uint8_t relay_num, uint16_t power)
{
    // 确保持久化模块已初始化
    persistence_init(RELAY_PERSISTENCE_NS);

    // 设置继电器最大功率
    uint8_t relay_index = relay_num - 1;
    relay_max_powers[relay_index] = power;

    // 持久化
    const char *key = nullptr;
    switch (relay_num)
    {
    case 1:
        key = RELAY1_MAX_POWER_KEY;
        break;
    case 2:
        key = RELAY2_MAX_POWER_KEY;
        break;
    case 3:
        key = RELAY3_MAX_POWER_KEY;
        break;
    }
    if (key)
    {
        persistence_put_bytes(key, &power, sizeof(power));
    }

    // persistence_end();
}

/**
 * @brief 获取指定继电器的最大功率
 * @param relay_num 要查询的继电器编号 (1, 2, 3)
 * @return uint16_t 继电器的最大功率值 (W)
 */
uint16_t ELECTRIC_RELAY_get_max_power(uint8_t relay_num)
{
    // 直接返回内部存储的状态
    return relay_max_powers[relay_num - 1];
}