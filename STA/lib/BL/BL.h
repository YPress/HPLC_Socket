#ifndef BL_H
#define BL_H

#include <Arduino.h>

// IO口
#define BL Serial1
#define BL_TX 17
#define BL_RX 18

// BL0906 UART 读/写操作帧控制码
#define BL0906_READ_CMD 0x35
#define BL0906_WRITE_CMD 0xCA

// 超时时间
#define BL0906_READ_TIMEOUT 100

/**
 * @brief 初始化BL0906电能计量芯片串口
 */
void BL_init();

/**
 * @brief 从BL0906读取指定寄存器的3字节数据
 * @param address 要读取的寄存器地址
 * @param dataBuffer 指向用于存储读取数据的3字节数组的指针
 * @return bool 读取成功返回true，失败返回false
 */
bool BL_read_register(byte address, byte *dataBuffer);

/**
 * @brief 向BL0906指定寄存器写入3字节数据
 * @param address 要写入的寄存器地址
 * @param dataBuffer 指向包含要写入数据的3字节数组的指针
 * @return bool 发送成功返回true，失败返回false
 */
bool BL_write_register(byte address, const byte *dataBuffer);

#endif