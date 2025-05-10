#ifndef HPLC_H
#define HPLC_H

#include <Arduino.h>
#include <Global.h>

// IO口
#define HPLC Serial2
#define HPLC_TX 8
#define HPLC_RX 3

/**
 * @brief 初始化HPLC模块
 */
void HPLC_init();

/**
 * @brief 处理接收到的数据帧
 * @param data 接收到的数据
 * @param callback 回调函数
 */
void HPLC_process_frame(uint8_t data, FrameCallbackFunc callback);

/**
 * @brief 发送数据帧
 * @param target_address 目标地址
 * @param frame 数据帧
 * @param frame_length 数据帧长度
 * @param is_ack_needed 是否需要ACK
 * @return true 发送成功
 * @return false 发送失败
 */
bool HPLC_send_frame(uint8_t target_address[], uint8_t frame[], int frame_length, bool is_ack_needed);

/**
 * @brief 发送心跳包
 * @param target_address 目标地址
 * @return true 发送成功
 * @return false 发送失败
 */
bool HPLC_send_heart_beat(uint8_t target_address[]);

/**
 * @brief 回复心跳包
 * @param target_address 目标地址
 */
void HPLC_reply_heart_beat(uint8_t target_address[]);

/**
 * @brief 获取网络拓扑中STA设备的MAC地址列表
 * @param sta_mac_list 存储STA设备MAC地址的数组
 * @param sta_count 存储STA设备数量的指针
 * @return true 获取成功
 * @return false 获取失败
 */
bool HPLC_get_topo_sta_mac_list(uint8_t sta_mac_list[][6], uint8_t *sta_count);

#endif