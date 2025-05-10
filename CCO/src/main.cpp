#include <Arduino.h>
#include <Global.h>
#include <HPLC.h>
#include <TJC.h>
#include <PowerStrip.h>
#include <BLRegConv.h>

// STA监控间隔 (毫秒)
#define STA_MONITOR_INTERVAL_MS 10000

// 目标通讯地址
uint8_t TARGET_ADDRESS[6] = {0x00, 0x13, 0xd7, 0x63, 0x22, 0x03};
// 当前页面对应的STA的MAC地址
uint8_t currMacAddr[6];

// STA监控任务的任务句柄
TaskHandle_t staMonitorTaskHandle = NULL;

// TJC串口访问互斥锁
SemaphoreHandle_t tjcMutex;
// HPLC串口访问互斥锁
SemaphoreHandle_t hplcMutex;

void monitorSTADevicesTask(void *pvParameters);
void TJC_handle_valid_frame(FrameParser frameParser);
void HPLC_handle_valid_frame(FrameParser frameParser);

void setup()
{
    // 初始化串口监视器
    Serial.begin(115200, SERIAL_8N1);

    // 初始化串口屏
    TJC_init();

    // 初始化载波模块串口
    HPLC_init();

    // 初始化排插管理器并加载数据
    PowerStrip_init();

    // 创建TJC串口访问互斥锁
    tjcMutex = xSemaphoreCreateMutex();
    if (tjcMutex == NULL)
    {
        Serial.println("初始化 -> TJC互斥锁创建失败");
        // 挂起
        while (1)
            ;
    }
    else
    {
        Serial.println("初始化 -> TJC互斥锁创建成功");
    }

    // 创建HPLC串口访问互斥锁
    hplcMutex = xSemaphoreCreateMutex();
    if (hplcMutex == NULL)
    {
        Serial.println("初始化 -> HPLC互斥锁创建失败");
        // 挂起
        while (1)
            ;
    }
    else
    {
        Serial.println("初始化 -> HPLC互斥锁创建成功");
    }

    // ESP32-S3：核心0 (PRO_CPU) 和核心1 (APP_CPU)【Arduino 的 loop 函数在核心1上运行】
    // 创建并启动STA监控任务【固定到核心0运行】
    BaseType_t taskCreated = xTaskCreatePinnedToCore(
        monitorSTADevicesTask, /* 任务函数 */
        "STAMonitorTask",      /* 任务名称字符串 */
        4096,                  /* 堆栈大小（字节） */
        NULL,                  /* 传递给任务的参数 */
        1,                     /* 任务优先级（0为最低） */
        &staMonitorTaskHandle, /* 任务句柄 */
        0                      /* 任务运行的核心（0代表PRO_CPU）*/
    );
    if (taskCreated == pdPASS)
    {
        Serial.println("初始化 -> STA监控任务 -> 创建并启动成功");
    }
    else
    {
        Serial.println("初始化 -> STA监控任务 -> 创建并启动失败");
    }
}

void loop()
{
    // TJC触摸屏信息交互
    // 尝试获取HPLC互斥锁，设置一个较短的超时时间以避免loop()长时间阻塞
    if (xSemaphoreTake(tjcMutex, (TickType_t)10) == pdTRUE)
    {
        while (TJC.available())
        {
            byte data = TJC.read();
            // print_to_serial_monitor("TJC", data);
            TJC_process_frame(data, TJC_handle_valid_frame);
        }
        // 释放TJC互斥锁
        xSemaphoreGive(tjcMutex);
    }

    // 载波模块信息交互
    // 尝试获取HPLC互斥锁，设置一个较短的超时时间以避免loop()长时间阻塞
    if (xSemaphoreTake(hplcMutex, (TickType_t)10) == pdTRUE)
    {
        while (HPLC.available())
        {
            byte data = HPLC.read();
            // print_to_serial_monitor("HPLC", data);
            HPLC_process_frame(data, HPLC_handle_valid_frame);
        }
        // 释放HPLC互斥锁
        xSemaphoreGive(hplcMutex);
    }
}

/**
 * @brief STA监控任务
 * @param pvParameters 任务参数 (未使用)
 */
void monitorSTADevicesTask(void *pvParameters)
{
    Serial.printf("STA监控任务 -> 在核心 %d 上启动\n", xPortGetCoreID());

    // STA设备MAC地址列表(3个设备，每个mac地址6字节)
    uint8_t sta_mac_list[3][6];
    // STA设备数量
    uint8_t sta_count;

    for (;;)
    {
        // 尝试获取HPLC互斥锁
        if (xSemaphoreTake(hplcMutex, portMAX_DELAY) == pdTRUE)
        {
            Serial.println("STA监控任务PART1启动 -> 已获取HPLC互斥锁");

            Serial.println("STA监控任务 -> 从CCO获取STA列表...");
            if (HPLC_get_topo_sta_mac_list(sta_mac_list, &sta_count))
            {
                Serial.printf("STA监控任务 -> 获取到 %d 个STA\n", sta_count);
                // 将新的STA添加到PowerStrip管理器
                for (int i = 0; i < sta_count; i++)
                {
                    PowerStrip strip;
                    bool found = PowerStrip_get(sta_mac_list[i], strip);
                    if (!found)
                    {
                        Serial.print("STA监控任务 -> 添加STA -> ");
                        for (int j = 0; j < 6; j++)
                            Serial.printf("%02X%s", sta_mac_list[i][j], j < 5 ? ":" : "");
                        Serial.println();

                        // 创建新的PowerStrip对象
                        PowerStrip newStrip;
                        memcpy(newStrip.macAddress, sta_mac_list[i], 6);
                        char default_name[20];
                        sprintf(default_name, "排插_%d", i + 1);
                        newStrip.name = String(default_name);
                        for (int k = 0; k < 3; k++)
                        {
                            newStrip.sockets[k] = {false, 0};
                        }
                        newStrip.isOnline = false;

                        // 添加到管理器
                        if (PowerStrip_add(newStrip))
                        {
                            Serial.println("STA监控任务 -> 成功添加新的排插到管理器");
                        }
                        else
                        {
                            Serial.println("STA监控任务 -> 添加新的排插到管理器失败");
                        }
                    }
                }
            }
            else
            {
                Serial.println("STA监控任务 -> 从CCO获取STA列表失败");
            }

            // 对所有已管理的STA设备进行心跳检测
            std::vector<PowerStrip> allStrips = PowerStrip_get_all();
            Serial.printf("STA监控任务 -> 开始对内存中的 %d 个排插进行心跳检测\n", allStrips.size());
            for (PowerStrip &strip_instance : allStrips)
            {
                Serial.print("STA监控任务 -> 检测STA -> ");
                for (int j = 0; j < 6; j++)
                    Serial.printf("%02X%s", strip_instance.macAddress[j], j < 5 ? ":" : "");
                Serial.println();

                // 发送心跳包进行检测
                bool is_currently_online = HPLC_send_heart_beat(strip_instance.macAddress);
                if (is_currently_online)
                {
                    Serial.println(" -- Online");
                }
                else
                {
                    Serial.println(" -- Offline");
                }

                // 仅当状态发生变化时更新
                if (strip_instance.isOnline != is_currently_online)
                {
                    strip_instance.isOnline = is_currently_online;
                    PowerStrip_update(strip_instance);
                }
            }

            // 释放HPLC互斥锁
            xSemaphoreGive(hplcMutex);
            Serial.println("STA监控任务PART1结束 -> 已释放HPLC互斥锁");
        }
        else
        {
            Serial.println("STA监控任务 -> 获取HPLC互斥锁超时或失败");
            // 获取互斥锁失败，等待100ms重试
            vTaskDelay(pdMS_TO_TICKS(100));
        }

        // 尝试获取TJC互斥锁
        if (xSemaphoreTake(tjcMutex, portMAX_DELAY) == pdTRUE)
        {
            Serial.println("STA监控任务PART2启动 -> 已获取TJC互斥锁");
            // 更新TJC触摸屏上的STA列表
            Serial.println("STA监控任务 -> 更新TJC触摸屏上的STA列表...");
            // TJC串口屏主页按钮下标
            int i = 1;
            // 遍历所有排插
            std::vector<PowerStrip> allStrips = PowerStrip_get_all();
            for (PowerStrip &strip_instance : allStrips)
            {
                Serial.print("STA监控任务 -> 处理STA -> ");
                for (int j = 0; j < 6; j++)
                    Serial.printf("%02X%s", strip_instance.macAddress[j], j < 5 ? ":" : "");
                Serial.println();

                if (strip_instance.isOnline)
                {
                    // 更新TJC触摸屏上的STA列表
                    TJC_set_property("Home", (String("p") + i).c_str(), "y", "95");
                    TJC_set_property("Home", (String("sname") + i).c_str(), "y", "105");
                    TJC_set_property("Home", (String("ps") + i).c_str(), "y", "130");

                    TJC_set_property("Home", (String("sname") + i).c_str(), "txt", strip_instance.name);
                    TJC_set_property("Home", (String("pmac") + i).c_str(), "txt", mac_to_string(strip_instance.macAddress));

                    i++;
                }
            }

            // 如果小于3个排插，则隐藏后续按钮
            while (i <= 3)
            {
                // 隐藏按钮
                TJC_set_property("Home", (String("p") + i).c_str(), "y", "295");
                TJC_set_property("Home", (String("sname") + i).c_str(), "y", "305");
                TJC_set_property("Home", (String("ps") + i).c_str(), "y", "330");

                TJC_set_property("Home", (String("sname") + i).c_str(), "txt", "排插");
                TJC_set_property("Home", (String("pmac") + i).c_str(), "txt", "");

                i++;
            }

            // 释放TJC互斥锁
            xSemaphoreGive(tjcMutex);
            Serial.println("STA监控任务PART2结束 -> 已释放TJC互斥锁");
        }
        else
        {
            Serial.println("STA监控任务 -> 获取TJC互斥锁超时或失败");
            // 获取互斥锁失败，等待100ms重试
            vTaskDelay(pdMS_TO_TICKS(100));
        }

        // 等待10秒后再次执行
        vTaskDelay(pdMS_TO_TICKS(STA_MONITOR_INTERVAL_MS));
    }
}

/**
 * TJC模块处理有效帧
 */
void TJC_handle_valid_frame(FrameParser frameParser)
{
    // Serial.println("HANDLE_VALID_FRAME:");
    // for (int i = 0; i < frameParser.index; i++)
    // {
    //     print_to_serial_monitor("TJC", frameParser.buffer[i]);
    // }
    // Serial.println();

    // 临时变量
    uint8_t macAddr[6]; // MAC地址Buffer
    PowerStrip strip;   // 排插对象Buffer
    // 设置STA推送开关的数据帧
    uint8_t pushFrame[3] = {
        0x13, // 控制码
        0x01, // 数据域长度
        0x00  // 推送开关状态
    };

    // 提取关键字段
    uint8_t ctrlCode = frameParser.buffer[5]; // 控制码
    uint8_t dataLen = frameParser.buffer[6];  // 数据域长度

    // 针对不同帧控制码执行不同操作
    switch (ctrlCode)
    {
    case 0x01:
        // 测试1
        Serial.println("测试1-获取网络拓扑节点数量");
        // HPLC.print("AT+TOPONUM?\r\n");
        break;

    case 0x02:
        // 测试2
        Serial.println("测试2-获取网络拓扑节点信息");
        // HPLC.print("AT+TOPOINFO=0,4\r\n");
        break;

    case 0x11:
        // 请求前往[Wifi设置/信息页面]
        Serial.println("请求前往[Wifi设置/信息页面]");
        TJC_goto_page("Wifi");
        break;

    case 0x12:
        // 前往[排插控制页面]
        Serial.println("前往[排插控制页面]");
        // 提取MAC地址
        for (int i = 0; i < 6; i++)
        {
            macAddr[i] = frameParser.buffer[7 + i];
        }
        // 获取排插信息
        if (PowerStrip_get(macAddr, strip))
        {
            // 属性设置给TJC串口屏
            TJC_set_property("Control", "mac", "txt", mac_to_string(strip.macAddress));
            TJC_set_property("Control", "sname", "txt", strip.name);
            TJC_set_property("Control", "bt1", "val", strip.sockets[0].state ? "1" : "0");
            TJC_set_property("Control", "bt2", "val", strip.sockets[1].state ? "1" : "0");
            TJC_set_property("Control", "bt3", "val", strip.sockets[2].state ? "1" : "0");
            TJC_set_property("Control", "xz1", "val", String(strip.sockets[0].maxPower));
            TJC_set_property("Control", "xz2", "val", String(strip.sockets[1].maxPower));
            TJC_set_property("Control", "xz3", "val", String(strip.sockets[2].maxPower));
            Serial.printf("MAC -> %s\n", mac_to_string(macAddr).c_str());
        }
        // 设置STA推送开关 - 开启
        pushFrame[2] = 0x01;
        // 发送帧
        if (HPLC_send_frame(macAddr, pushFrame, sizeof(pushFrame), true))
        {
            // 发送成功，设置当前页面的STA的MAC地址
            memcpy(currMacAddr, macAddr, 6);
        }
        else
        {
            // 发送失败，回滚串口屏，返回主页面
            TJC_click("back", "0");
        }
        break;

    case 0x21:
        // 设置 Wifi SSID
        Serial.println("设置 Wifi SSID");
        for (int i = 0; i < dataLen; i++)
        {
            print_to_serial_monitor("SSID", frameParser.buffer[7 + i]);
        }
        break;

    case 0x22:
        // 设置 Wifi 密码
        Serial.println("设置 Wifi 密码");
        for (int i = 0; i < dataLen; i++)
        {
            print_to_serial_monitor("PWD", frameParser.buffer[7 + i]);
        }
        break;

    case 0x23:
        // 从[Wifi设置页面]回到[主页面]
        Serial.println("从[Wifi设置页面]回到[主页面]");
        break;

    case 0x31:
        // 断开 Wifi 连接，前往[Wifi设置页面]
        Serial.println("断开 Wifi 连接，前往[Wifi设置页面]");
        break;

    case 0x32:
        // 从[Wifi信息页面]回到[主页面]
        Serial.println("从[Wifi信息页面]回到[主页面]");
        break;

    case 0x41:
        // 设置排插名称
        Serial.println("设置排插名称");
        // 提取MAC地址
        for (int i = 0; i < 6; i++)
        {
            macAddr[i] = frameParser.buffer[7 + i];
        }
        // 获取排插信息
        if (PowerStrip_get(macAddr, strip))
        {
            // 提取名称
            char nameBuffer[dataLen - 6 + 1];
            for (int i = 0; i < dataLen - 6; i++)
            {
                nameBuffer[i] = frameParser.buffer[13 + i];
            }
            nameBuffer[dataLen - 6] = '\0';
            // 更新排插名称
            strip.name = String(nameBuffer);
            PowerStrip_update(strip);
            Serial.printf("MAC -> %s | NAME -> %s\n", mac_to_string(macAddr).c_str(), strip.name.c_str());
        }
        break;

    case 0x42:
        // 设置指定插孔开关状态
        Serial.println("设置指定插孔开关状态");
        // 提取MAC地址
        for (int i = 0; i < 6; i++)
        {
            macAddr[i] = frameParser.buffer[7 + i];
        }
        // 获取排插信息
        if (PowerStrip_get(macAddr, strip))
        {
            // 提取插孔ID和开关状态
            uint8_t socketId = frameParser.buffer[13];
            bool socketState = (frameParser.buffer[14] == 0x01);
            // 设置STA插孔状态
            uint8_t frame[4] = {
                0x11,                  // 控制码
                0x02,                  // 数据域长度
                socketId,              // 插孔ID
                frameParser.buffer[14] // 插孔状态
            };
            // 发送帧
            if (HPLC_send_frame(macAddr, frame, sizeof(frame), true))
            {
                // 发送成功，更新插孔状态
                strip.sockets[socketId - 1].state = socketState;
                PowerStrip_update(strip);
                // 更新串口屏显示内容
                TJC_set_property("Control", (String("dl") + socketId).c_str(), "txt", "-"); // 电流显示为"-"
                TJC_set_property("Control", (String("gl") + socketId).c_str(), "txt", "-"); // 功率显示为"-"
                Serial.printf("MAC -> %s | SOCKET_ID -> %d | STATE -> %s\n", mac_to_string(macAddr).c_str(), socketId, socketState ? "ON" : "OFF");
            }
            else
            {
                // 发送失败，回滚串口屏对应按钮状态
                TJC_set_property("Control", (String("bt") + socketId).c_str(), "val", strip.sockets[socketId - 1].state ? "1" : "0");
            }
        }
        break;

    case 0x43:
        // 设置指定插孔最大功率
        Serial.println("设置指定插孔最大功率");
        // 提取MAC地址
        for (int i = 0; i < 6; i++)
        {
            macAddr[i] = frameParser.buffer[7 + i];
        }
        // 获取排插信息
        if (PowerStrip_get(macAddr, strip))
        {
            // 提取插孔ID和最大功率
            uint8_t socketId = frameParser.buffer[13];
            uint8_t powerLowByte = frameParser.buffer[14];
            uint8_t powerHighByte = frameParser.buffer[15];
            uint16_t maxPower = (powerHighByte << 8) | powerLowByte;
            // 设置STA插孔最大功率
            uint8_t frame[5] = {
                0x12,         // 控制码
                0x03,         // 数据域长度
                socketId,     // 插孔ID
                powerLowByte, // 最大功率低字节
                powerHighByte // 最大功率高字节
            };
            // 发送帧
            if (HPLC_send_frame(macAddr, frame, sizeof(frame), true))
            {
                // 发送成功，更新插孔最大功率
                strip.sockets[socketId - 1].maxPower = maxPower;
                PowerStrip_update(strip);
                Serial.printf("MAC -> %s | SOCKET_ID -> %d | MAX_POWER -> %d\n", mac_to_string(macAddr).c_str(), socketId, maxPower);
            }
            else
            {
                // 发送失败，回滚串口屏对应功率设置
                TJC_set_property("Control", (String("xz") + socketId).c_str(), "val", String(strip.sockets[socketId - 1].maxPower));
            }
        }
        break;

    case 0x44:
        // 从[排插控制页面]回到[主页面]
        Serial.println("从[排插控制页面]回到[主页面]");
        // 设置STA推送开关 - 关闭
        pushFrame[2] = 0x00;
        // 发送帧
        HPLC_send_frame(currMacAddr, pushFrame, sizeof(pushFrame), true);
        // 清空当前页面的STA的MAC地址
        memset(currMacAddr, 0, sizeof(currMacAddr));
        break;

    default:
        break;
    }
}

/**
 * HPLC载波模块处理有效帧
 */
void HPLC_handle_valid_frame(FrameParser frameParser)
{
    // Serial.println("HANDLE_VALID_FRAME:");
    // for (int i = 0; i < frameParser.index; i++)
    // {
    //     print_to_serial_monitor("HPLC", frameParser.buffer[i]);
    // }
    // Serial.println();

    // 临时变量
    uint8_t macAddr[6];        // MAC地址Buffer
    PowerStrip strip;          // 排插对象Buffer
    uint8_t socketId;          // 插孔ID
    uint8_t bl_data_buffer[3]; // BL0906 数据缓冲区 (3 字节)
    // 响应ACK帧
    uint8_t ackFrame[2] = {
        0x00, // 控制码占位
        0x00  // 数据域长度
    };

    // 提取关键字段
    uint8_t ctrlCode = frameParser.buffer[5]; // 控制码
    uint8_t dataLen = frameParser.buffer[6];  // 数据域长度

    // 针对不同帧控制码执行不同操作
    switch (ctrlCode)
    {
    case 0x66:
        // 回复心跳包
        HPLC_reply_heart_beat(TARGET_ADDRESS);
        break;

    case 0x13:
        // 接收STA功率超限通知
        Serial.println("接收STA功率超限通知");
        // 提取MAC地址
        for (int i = 0; i < 6; i++)
        {
            macAddr[i] = frameParser.buffer[7 + i];
        }
        // 获取排插信息
        if (PowerStrip_get(macAddr, strip))
        {
            // 提取插孔ID
            socketId = frameParser.buffer[13];
            // 处理功率超限通知 -> 跳闸了
            // 更新排插状态并保存
            strip.sockets[socketId - 1].state = false;
            PowerStrip_update(strip);
            // 比较是否是当前页面的STA
            if (memcmp(macAddr, currMacAddr, 6) == 0)
            {
                // 更新串口屏显示
                TJC_set_property("Control", (String("bt") + socketId).c_str(), "val", "0"); // 关闭按钮
                TJC_set_property("Control", (String("dl") + socketId).c_str(), "txt", "-"); // 电流显示为"-"
                // 功率保留
            }
        }
        // 响应ACK帧
        ackFrame[0] = 0x93;
        // 发送ACK帧
        HPLC_send_frame(macAddr, ackFrame, sizeof(ackFrame), false);
        break;

    case 0x14:
        // 接收STA插孔电流
        Serial.println("接收STA插孔电流");
        // 提取MAC地址
        for (int i = 0; i < 6; i++)
        {
            macAddr[i] = frameParser.buffer[7 + i];
        }
        // 比较是否是当前页面的STA
        if (memcmp(macAddr, currMacAddr, 6) == 0)
        {
            // 提取插孔ID
            socketId = frameParser.buffer[13];
            // 提取电流数据
            bl_data_buffer[0] = frameParser.buffer[14];
            bl_data_buffer[1] = frameParser.buffer[15];
            bl_data_buffer[2] = frameParser.buffer[16];
            float current_reg = ((uint32_t)bl_data_buffer[2] << 16) | ((uint32_t)bl_data_buffer[1] << 8) | bl_data_buffer[0];
            // 转换为实际电流 (A)
            float current = BL_currentRegister2ActualCurrent(current_reg);
            // 显示到串口屏
            TJC_set_property("Control", (String("dl") + socketId).c_str(), "txt", String(current));
        }
        break;

    case 0x15:
        // 接收STA插孔功率
        Serial.println("接收STA插孔功率");
        // 提取MAC地址
        for (int i = 0; i < 6; i++)
        {
            macAddr[i] = frameParser.buffer[7 + i];
        }
        // 比较是否是当前页面的STA
        if (memcmp(macAddr, currMacAddr, 6) == 0)
        {
            // 提取插孔ID
            socketId = frameParser.buffer[13];
            // 提取功率数据
            bl_data_buffer[0] = frameParser.buffer[14];
            bl_data_buffer[1] = frameParser.buffer[15];
            bl_data_buffer[2] = frameParser.buffer[16];
            uint32_t power_reg = ((uint32_t)bl_data_buffer[2] << 16) | ((uint32_t)bl_data_buffer[1] << 8) | bl_data_buffer[0];
            // 转换为实际功率 (W)
            float power = BL_powerRegister2ActualPower(power_reg);
            // 显示到串口屏
            TJC_set_property("Control", (String("gl") + socketId).c_str(), "txt", String(power));
        }
        break;
        
    default:
        break;
    }
}