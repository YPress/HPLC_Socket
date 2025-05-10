#ifndef ELECTRIC_RELAY_H
#define ELECTRIC_RELAY_H

#include <Arduino.h>

// 3个继电器控制IO口
#define ELECTRIC_RELAY_PIN_1 4
#define ELECTRIC_RELAY_PIN_2 5
#define ELECTRIC_RELAY_PIN_3 6

/**
 * @brief 初始化继电器控制
 * @details 初始化所有继电器的引脚，尝试从持久化存储中加载每个继电器的状态，并将其应用到对应的引脚
 */
void ELECTRIC_RELAY_init();

/**
 * @brief 设置指定继电器的状态
 * @param relay_num 要控制的继电器编号 (1, 2, 3)
 * @param state 要设置的状态 (0 断开 - 低电平, 1 吸合 - 高电平)
 */
void ELECTRIC_RELAY_control(uint8_t relay_num, uint8_t state);

/**
 * @brief 获取指定继电器的状态
 * @param relay_num 要查询的继电器编号 (1, 2, 3)
 * @return uint8_t 继电器的当前状态 (0 或 1)
 */
uint8_t ELECTRIC_RELAY_get_state(uint8_t relay_num);

/**
 * @brief 设置指定继电器的最大功率
 * @param relay_num 要设置的继电器编号 (1, 2, 3)
 * @param power 要设置的最大功率值 (W)
 */
void ELECTRIC_RELAY_set_max_power(uint8_t relay_num, uint16_t power);

/**
 * @brief 获取指定继电器的最大功率
 * @param relay_num 要查询的继电器编号 (1, 2, 3)
 * @return uint16_t 继电器的最大功率值 (W)
 */
uint16_t ELECTRIC_RELAY_get_max_power(uint8_t relay_num);

#endif