#include <TJC.h>

// 通用请求/应答帧头
static uint8_t FRAME_HEAD[5] = {
    FRAME_LEAD_BYTE, FRAME_LEAD_BYTE, FRAME_LEAD_BYTE, FRAME_LEAD_BYTE, // 前导字节
    FRAME_HEADER                                                        // 帧起始符
};

// 通用请求/应答帧尾
static uint8_t FRAME_TAIL[1] = {
    FRAME_END // 帧结束符
};

// 创建[帧解析器]
static FrameParser frameParser;

/**
 * 加入解析器
 */
static void add_parser(uint8_t data)
{
    // 加入[帧内容缓冲区]
    frameParser.buffer[frameParser.index++] = data;
}

/**
 * 重置解析器
 */
static void reset_parser()
{
    frameParser.state = WAIT_LEAD_BYTE;           // [状态]回到初始
    memset(frameParser.buffer, 0, MAX_FRAME_LEN); // [帧内容缓冲区]数组清零
    frameParser.index = 0;                        // [帧内容缓冲区下标]归零
    frameParser.dataFieldEndIndex = 0;            // [数据域结束下标]归零
}

/**
 * @brief 初始化TJC串口屏模块
 */
void TJC_init()
{
    // 初始化串口
    TJC.begin(115200, SERIAL_8N1, TJC_RX, TJC_TX);
    // 初始化[帧解析器]
    reset_parser();
    // 发送命令让屏幕跳转到[主页面]
    TJC.printf("page Home\xff\xff\xff");
}

/**
 * @brief 处理接收到的数据帧
 * @param data 接收到的数据
 * @param callback 回调函数
 */
void TJC_process_frame(uint8_t data, FrameCallbackFunc callback)
{
    switch (frameParser.state)
    {
    case WAIT_LEAD_BYTE: // [前导字节检测]状态: 4个 FE（4字节）
        // Serial.println("WAIT_LEAD_BYTE");
        if (data != FRAME_LEAD_BYTE)
        {
            // 没收到前导字节就重置解析器
            reset_parser();
        }
        else
        {
            // 解析器装入新数据
            add_parser(data);
            if (frameParser.index == 4)
            {
                // 前导字节读完后进入下一个状态
                frameParser.state = WAIT_HEADER;
            }
        }
        break;

    case WAIT_HEADER: // [第1个起始符检测]状态: 第1个起始符（1字节）
        // Serial.println("WAIT_HEADER");
        if (data != FRAME_HEADER)
        {
            // 没收到起始符就重置解析器
            reset_parser();
        }
        else
        {
            // 解析器装入新数据
            add_parser(data);
            // 进入下一个状态
            frameParser.state = READING_CTRL;
        }
        break;

    case READING_CTRL: // [控制码读取]状态: 控制码（1字节）
        // Serial.println("READING_CTRL");
        // 解析器装入新数据
        add_parser(data);
        // 进入下一个状态
        frameParser.state = READING_DATA_LEN;
        break;

    case READING_DATA_LEN: // [数据域长度读取]状态: 数据域长度（1字节）
        // Serial.println("READING_DATA_LEN");
        // 解析器装入新数据
        add_parser(data);
        // 设置[数据域结束下标] = 通用请求/应答帧头长度 + 1位控制码 + 数据域长度
        frameParser.dataFieldEndIndex = ARRAY_LENGTH(FRAME_HEAD) + 1 + data;
        if (data == 0x00)
        {
            // 没有数据，跳过下一个状态，直接进入下下一个状态
            frameParser.state = WAIT_EOF;
        }
        else
        {
            // 有数据，进入下一个状态
            frameParser.state = READING_DATA;
        }
        break;

    case READING_DATA: // [数据域读取]状态:
        // Serial.println("READING_DATA");
        // 解析器装入新数据
        add_parser(data);
        if (frameParser.index > frameParser.dataFieldEndIndex)
        {
            // 读完数据域进入下一个状态
            frameParser.state = WAIT_EOF;
        }
        break;

    case WAIT_EOF: // [结束符检查]状态:
        // Serial.println("WAIT_EOF");
        if (data == FRAME_END)
        {
            // 解析器装入新数据
            add_parser(data);
            // 收到完整帧并校验通过 -> 执行回调（检查空指针避免崩溃）
            if (callback)
            {
                callback(frameParser);
            }
        }
        // 重置解析器
        reset_parser();
        break;
    }
}

/**
 * @brief 跳转到指定页面
 * @param page_name 页面名称
 */
void TJC_goto_page(const char *page_name)
{
    TJC.printf("page %s\xff\xff\xff", page_name);
}

/**
 * @brief 触发点击事件
 * @param control_name 控件名称
 * @param value 触发内容 ("0" - 弹起事件 或 "1" - 按下事件)
 */
void TJC_click(const char *control_name, const String value)
{
    TJC.printf("click %s,%s\xff\xff\xff", control_name, value);
}

/**
 * @brief 设置指定页面控件的属性值
 * @param page_name 页面名称
 * @param control_name 控件名称
 * @param property_name 属性名称 ("val" 或 "txt" 或 "aph")
 * @param value 要设置的值
 */
void TJC_set_property(const char *page_name, const char *control_name, const char *property_name, const String value)
{
    if (strcmp(property_name, "val") == 0 || strcmp(property_name, "aph") == 0 || strcmp(property_name, "y") == 0)
    {
        // 数字类型属性，不加双引号
        TJC.printf("%s.%s.%s=%s\xff\xff\xff", page_name, control_name, property_name, value);
        // Serial.printf("%s.%s.%s=%s\xff\xff\xff", page_name, control_name, property_name, value);
    }
    else if (strcmp(property_name, "txt") == 0)
    {
        // 字符串类型属性，加双引号
        TJC.printf("%s.%s.%s=\"%s\"\xff\xff\xff", page_name, control_name, property_name, value);
        // Serial.printf("%s.%s.%s=\"%s\"\xff\xff\xff", page_name, control_name, property_name, value);
    }
}

/**
 * @brief 加指定页面控件的属性值
 * @param page_name 页面名称
 * @param control_name 控件名称
 * @param property_name 属性名称 ("val" 或 "txt" 或 "y")
 * @param value 要加的值
 */
void TJC_plus_property(const char *page_name, const char *control_name, const char *property_name, const String value)
{
    if (strcmp(property_name, "val") == 0 || strcmp(property_name, "aph") == 0 || strcmp(property_name, "y") == 0)
    {
        // 数字类型属性，不加双引号
        TJC.printf("%s.%s.%s+=%s\xff\xff\xff", page_name, control_name, property_name, value);
        // Serial.printf("%s.%s.%s+=%s\xff\xff\xff", page_name, control_name, property_name, value);
    }
    else if (strcmp(property_name, "txt") == 0)
    {
        // 字符串类型属性，加双引号
        TJC.printf("%s.%s.%s+=\"%s\"\xff\xff\xff", page_name, control_name, property_name, value);
        // Serial.printf("%s.%s.%s+=\"%s\"\xff\xff\xff", page_name, control_name, property_name, value);
    }
}

/**
 * @brief 减指定页面控件的属性值
 * @param page_name 页面名称
 * @param control_name 控件名称
 * @param property_name 属性名称 ("val" 或 "txt" 或 "y")
 * @param value 要减的值
 */
void TJC_minus_property(const char *page_name, const char *control_name, const char *property_name, const String value)
{
    if (strcmp(property_name, "val") == 0 || strcmp(property_name, "aph") == 0 || strcmp(property_name, "y") == 0)
    {
        // 数字类型属性，不加双引号
        TJC.printf("%s.%s.%s-=%s\xff\xff\xff", page_name, control_name, property_name, value);
        // Serial.printf("%s.%s.%s+=%s\xff\xff\xff", page_name, control_name, property_name, value);
    }
    else if (strcmp(property_name, "txt") == 0)
    {
        // 字符串类型属性，加双引号
        TJC.printf("%s.%s.%s-=\"%s\"\xff\xff\xff", page_name, control_name, property_name, value);
        // Serial.printf("%s.%s.%s+=\"%s\"\xff\xff\xff", page_name, control_name, property_name, value);
    }
}