#ifndef TJC_H
#define TJC_H

#include <Arduino.h>
#include <Global.h>

// IO口
#define TJC Serial1
#define TJC_TX 17
#define TJC_RX 18

/**
 * @brief 初始化TJC串口屏模块
 */
void TJC_init();

/**
 * @brief 处理接收到的数据帧
 * @param data 接收到的数据
 * @param callback 回调函数
 */
void TJC_process_frame(uint8_t data, FrameCallbackFunc callback);

/**
 * @brief 跳转到指定页面
 * @param page_name 页面名称
 */
void TJC_goto_page(const char *page_name);

/**
 * @brief 触发点击事件
 * @param control_name 控件名称
 * @param value 触发内容 ("0" - 弹起事件 或 "1" - 按下事件)
 */
void TJC_click(const char *control_name, const String value);

/**
 * @brief 设置指定页面控件的属性值
 * @param page_name 页面名称
 * @param control_name 控件名称
 * @param property_name 属性名称 ("val" 或 "txt" 或 "aph")
 * @param value 要设置的值
 */
void TJC_set_property(const char *page_name, const char *control_name, const char *property_name, const String value);

/**
 * @brief 加指定页面控件的属性值
 * @param page_name 页面名称
 * @param control_name 控件名称
 * @param property_name 属性名称 ("val" 或 "txt" 或 "y")
 * @param value 要加的值
 */
void TJC_plus_property(const char *page_name, const char *control_name, const char *property_name, const String value);

/**
 * @brief 减指定页面控件的属性值
 * @param page_name 页面名称
 * @param control_name 控件名称
 * @param property_name 属性名称 ("val" 或 "txt" 或 "y")
 * @param value 要减的值
 */
void TJC_minus_property(const char *page_name, const char *control_name, const char *property_name, const String value);

#endif