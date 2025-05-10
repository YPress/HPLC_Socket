#include <HPLC.h>

// ACK机制的全局变量，指示是否已接收并验证ACK
volatile bool ackReceived = false;
// 最大重试次数
static int MAX_RETRIES = 3;
// ACK超时时间（毫秒）
static long ACK_TIMEOUT_MS = 1000;

// 通用请求/应答帧头
static uint8_t FRAME_HEAD[5] = {
    FRAME_LEAD_BYTE, FRAME_LEAD_BYTE, FRAME_LEAD_BYTE, FRAME_LEAD_BYTE, // 前导字节
    FRAME_HEADER                                                        // 帧起始符
};

// 通用请求/应答帧尾
static uint8_t FRAME_TAIL[2] = {
    0x00,     // 校验码占位符
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
    // 前导字节不计入校验和
    if (frameParser.state != WAIT_LEAD_BYTE)
    {
        // 加入[校验和]
        frameParser.checksum += data;
    }
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
    frameParser.checksum = 0;                     // [校验和]归零
}

/**
 * @brief 初始化HPLC模块
 */
void HPLC_init()
{
    // 初始化串口
    HPLC.begin(115200, SERIAL_8E1, HPLC_RX, HPLC_TX);
    // 初始化[帧解析器]
    reset_parser();
}

/**
 * @brief 处理接收到的数据帧
 * @param data 接收到的数据
 * @param callback 回调函数
 */
void HPLC_process_frame(uint8_t data, FrameCallbackFunc callback)
{
    // 校验码
    uint8_t calculatedCS;
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
            frameParser.state = READING_CHECKSUM;
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
            frameParser.state = READING_CHECKSUM;
        }
        break;

    case READING_CHECKSUM: // [校验和验证]状态:
        // Serial.println("READING_CHECKSUM");
        // 计算缓冲区的校验码
        calculatedCS = frameParser.checksum % 256;
        if (data != calculatedCS)
        {
            // 校验失败就重置解析器
            reset_parser();
        }
        else
        {
            // 解析器装入新数据
            add_parser(data);
            // 校验通过进入下一个状态
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
 * 根据发送的控制码返回期望的应答控制码
 */
uint8_t get_expected_ack_code(uint8_t sent_ctrl_code)
{
    switch (sent_ctrl_code)
    {
    case 0x66:
        // 心跳包
        return 0x88;
    case 0x11:
        // STA接收CCO -> 设置插孔通断状态
        return 0x91;
    case 0x12:
        // STA接收CCO -> 设置插孔最大功率
        return 0x92;
    case 0x13:
        // CCO接收STA -> 功率超限通知
        return 0x93;
    case 0x14:
        // CCO接收STA -> 插孔电流
        return 0x94;
    case 0x15:
        // CCO接收STA -> 插孔功率
        return 0x95;
    default:
        return 0x00;
    }
}

/**
 * @brief 发送数据帧
 * @param target_address 目标地址
 * @param frame 数据帧
 * @param frame_length 数据帧长度
 * @param is_ack_needed 是否需要ACK
 * @return true 发送成功
 * @return false 发送失败
 */
bool HPLC_send_frame(uint8_t target_address[], uint8_t frame[], int frame_length, bool is_ack_needed)
{
    // 校验和
    uint8_t cs = FRAME_HEADER;
    // 计算帧内容[校验和]（从第一个FRAME_HEADER到校验码前的所有字节和）
    for (int i = 0; i < frame_length; i++)
    {
        cs += frame[i];
    }
    // 填充最终[校验和]
    FRAME_TAIL[0] = cs % 256;
    // 计算数据总长度
    uint8_t len = (ARRAY_LENGTH(FRAME_HEAD) + frame_length + ARRAY_LENGTH(FRAME_TAIL));

    // frame[0] 是发送帧的控制码，通过该控制码获取期望的ACK控制码
    uint8_t expected_ack_ctrl_code = get_expected_ack_code(frame[0]);

    // 重试次数
    int retry_count = 0;
    // 重试逻辑
    do
    {
        // [AT命令]发送完整帧("AT+SEND=0013D7632202,30,FEFEFEFE6899063555960A4633AA16\r\n")
        HPLC.print("AT+SEND=");
        HPLC.print(mac_to_string(target_address)); // 目标 MAC 地址
        HPLC.print(",");
        HPLC.printf("%d", len); // 数据长度
        HPLC.print(",");
        HPLC.write(FRAME_HEAD, ARRAY_LENGTH(FRAME_HEAD)); // 通用请求/应答帧头
        HPLC.write(frame, frame_length);                  // 帧内容
        HPLC.write(FRAME_TAIL, ARRAY_LENGTH(FRAME_TAIL)); // 通用请求/应答帧尾
        HPLC.print("\r\n");

        if (!is_ack_needed)
        {
            // 如果不需要ACK，直接返回true
            return true;
        }

        // 重置应答接收标志
        ackReceived = false;

        // 记录发送时间
        long send_time = millis();
        // 在超时时间内等待ACK应答
        while (millis() - send_time < ACK_TIMEOUT_MS)
        {
            if (HPLC.available())
            {
                // 假设 HPLC_process_frame 会在收到完整帧后设置 ackReceived 和填充 ackFrameParser
                HPLC_process_frame(HPLC.read(), [expected_ack_ctrl_code](FrameParser frameParser) { // 使用 lambda 表达式捕获期望的ACK控制码
                    // 提取控制码
                    uint8_t ctrlCode = frameParser.buffer[5];
                    if (ctrlCode == expected_ack_ctrl_code)
                    {
                        // 收到了正确的ACK，设置全局标志
                        ackReceived = true;
                    }
                });
            }
            // 如果收到了正确的ACK，返回true
            if (ackReceived)
            {
                return true;
            }
            // 任务延时，防止看门狗超时导致重启
            vTaskDelay(pdMS_TO_TICKS(10));
        }

        // 上面没有返回，证明ACK超时
        Serial.print("等待ACK超时，正在重试... (");
        Serial.print(retry_count + 1);
        Serial.print("/");
        Serial.print(MAX_RETRIES);
        Serial.println(")");
        // 增加重试次数
        retry_count++;

    } while (!ackReceived && retry_count < MAX_RETRIES);

    // 超过最大重试次数，发送失败
    return false;
}

/**
 * @brief 发送心跳包
 * @param target_address 目标地址
 * @return true 发送成功
 * @return false 发送失败
 */
bool HPLC_send_heart_beat(uint8_t target_address[])
{
    // 心跳包数据帧
    uint8_t test_frame[2] = {
        0x66, // 控制码
        0x00  // 数据域长度
    };

    // 发送帧并返回结果
    return HPLC_send_frame(target_address, test_frame, ARRAY_LENGTH(test_frame), true);
}

/**
 * @brief 回复心跳包
 * @param target_address 目标地址
 */
void HPLC_reply_heart_beat(uint8_t target_address[])
{
    // 心跳包数据帧
    uint8_t test_frame[2] = {
        0x88, // 控制码
        0x00  // 数据域长度
    };

    // 发送帧
    HPLC_send_frame(target_address, test_frame, ARRAY_LENGTH(test_frame), false);
}

/**
 * @brief 获取网络拓扑中STA设备的MAC地址列表
 * @param sta_mac_list 存储STA设备MAC地址的数组
 * @param sta_count 存储STA设备数量的指针
 * @return true 获取成功
 * @return false 获取失败
 */
bool HPLC_get_topo_sta_mac_list(uint8_t sta_mac_list[][6], uint8_t *sta_count)
{
    String response = ""; // 串口响应的字符串
    long startTime;       // 超时开始时间
    char c;               // 串口字符Buffer
    int node_count = 0;   // 网络中的节点数量

    // 初始化STA设备数量为0
    *sta_count = 0;

    // 1. 发送 AT+TOPONUM? 指令获取网络中的节点数量
    HPLC.print("AT+TOPONUM?\r\n");

    bool topoNumOk = false; // 是否成功获取TOPONUM响应
    // 等待500ms超时
    startTime = millis();
    response = "";
    while (millis() - startTime < 500)
    {
        if (HPLC.available())
        {
            // 读取一个字符，追加到响应字符串
            c = HPLC.read();
            response += c;
            // 检查是否收到以 "\r+ok=" 开始并以 "\r\n" 结束的完整行
            if (response.indexOf("\r+ok=") != -1 && response.indexOf("\r\n") != -1)
            {
                topoNumOk = true;
                break;
            }
        }
        // 任务延时，防止看门狗超时导致重启
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    // 如果成功获取到TOPONUM响应
    if (topoNumOk)
    {
        // 找到 "\r+ok=" 的位置
        int ok_index = response.indexOf("\r+ok=");
        // 从 "\r+ok=" 之后找到第一个 "\r\n" 的位置
        int rn_index = response.indexOf("\r\n", ok_index);
        // 提取 "\r+ok=" 和 "\r\n" 之间的数字字符串
        String num_str = response.substring(ok_index + strlen("\r+ok="), rn_index);
        // 将数字字符串转换为整数
        node_count = num_str.toInt();
        Serial.printf("HPLC -> STA节点数量: %d\n", node_count);
    }
    else
    {
        Serial.println("HPLC -> 获取TOPONUM超时或错误");
        return false;
    }

    // 如果节点数量为0
    if (node_count == 0)
    {
        return true;
    }

    // 2. 发送 AT+TOPOINFO=1,node_count 指令获取指定数量的节点信息
    HPLC.print("AT+TOPOINFO=1,"); // 从第1个节点开始查询
    HPLC.print(node_count);       // 要查询的节点数量
    HPLC.print("\r\n");

    // 3. 解析响应，提取STA设备的MAC地址
    int parsed_mac_count = 0; // 已成功解析的MAC地址数量
    for (int i = 0; i < node_count; ++i)
    {
        bool lineOk = false; // 标记当前行是否成功接收

        // 每行响应等待500ms超时
        response = "";
        startTime = millis();
        while (millis() - startTime < 500)
        {
            if (HPLC.available())
            {
                // 读取一个字符，追加到响应字符串
                c = HPLC.read();
                response += c;
                // 检查是否收到以 "\r+ok=" 开始并以 "\r\n" 结束的完整行
                if (response.startsWith("\r+ok=") && response.endsWith("\r\n"))
                {
                    lineOk = true;
                    break;
                }
            }
            // 任务延时，防止看门狗超时导致重启
            vTaskDelay(pdMS_TO_TICKS(10));
        }

        // 如果成功接收到当前节点的响应行
        if (lineOk)
        {
            // 提取MAC地址: MAC地址位于 "\r+ok=" 之后，到第一个逗号 ',' 之前
            // MAC地址字符串的起始位置
            int mac_start_index = strlen("\r+ok=");
            // MAC地址字符串的结束位置(第一个逗号)
            int mac_end_index = response.indexOf(',', mac_start_index);
            // 如果找到了逗号
            if (mac_end_index != -1)
            {
                // 提取MAC地址字符串
                String mac_str = response.substring(mac_start_index, mac_end_index);
                // 将12字符的MAC地址字符串转换为6字节的uint8_t数组
                string_to_mac(mac_str, sta_mac_list[*sta_count]);
                // STA设备数量加1
                (*sta_count)++;
                // 已解析的MAC地址数量加1
                parsed_mac_count++;
            }
            else
            {
                Serial.println("HPLC -> 在TOPOINFO行中找不到MAC地址");
            }
        }
        else
        {
            Serial.println("HPLC -> 获取TOPOINFO行超时或错误");
            // 任何一行失败，则整体失败
            return false;
        }
    }

    if (parsed_mac_count > 0)
    {
        // 成功解析到至少一个MAC地址
        return true;
    }

    Serial.println("HPLC -> 未能从TOPOINFO响应中解析出任何MAC地址");
    return false;
}