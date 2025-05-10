#include <Arduino.h>
#include <HPLC.h>
#include <BL.h>
#include <BLRegConv.h>
#include <ElectricRelay.h>

// 电源监控间隔 (毫秒)
#define POWER_MONITOR_INTERVAL_MS 2000

// 本机STA通讯地址
uint8_t LOCAL_ADDRESS[6] = {0x00, 0x13, 0xd7, 0x63, 0x22, 0x02};
// 目标CCO通讯地址
uint8_t TARGET_ADDRESS[6] = {0x00, 0x13, 0xd7, 0x63, 0x22, 0x01};

// FreeRTOS 任务句柄
TaskHandle_t powerMonitoringTaskHandle = NULL;

// HPLC串口访问互斥锁
SemaphoreHandle_t hplcMutex;

// 继电器 1, 2, 3 的 BL0906 电流寄存器地址
const byte CURRENT_REGISTERS[] = {0x0D, 0x0E, 0x0F};
// 继电器 1, 2, 3 的 BL0906 功率寄存器地址
const byte POWER_REGISTERS[] = {0x23, 0x24, 0x25};
// 电能参数推送开关
bool electricParamPush = false;

void powerMonitoringTask(void *pvParameters);
void HPLC_handle_valid_frame(FrameParser frameParser);

void setup()
{
    // 初始化串口监视器
    Serial.begin(115200, SERIAL_8N1);

    // 初始化继电器控制
    ELECTRIC_RELAY_init();

    // 初始化载波模块串口
    HPLC_init();

    // 初始化电能计量芯片串口
    BL_init();

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
    // 创建并启动电源监控任务【固定到核心0运行】
    BaseType_t taskCreated = xTaskCreatePinnedToCore(
        powerMonitoringTask,        /* 任务函数 */
        "PowerMonitor",             /* 任务名称字符串 */
        4096,                       /* 堆栈大小 (字节) */
        NULL,                       /* 传递给任务的参数 */
        1,                          /* 任务优先级 */
        &powerMonitoringTaskHandle, /* 任务句柄 */
        0                           /* 任务运行的核心 */
    );
    if (taskCreated == pdPASS)
    {
        Serial.println("初始化 -> 电源监控任务 -> 创建并启动成功");
    }
    else
    {
        Serial.println("初始化 -> 电源监控任务 -> 创建并启动失败");
    }
}

void loop()
{
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
 * @brief 功率监控任务
 * @param pvParameters 任务参数 (未使用)
 */
void powerMonitoringTask(void *pvParameters)
{
    Serial.printf("功率监控任务 -> 在核心 %d 上启动\n", xPortGetCoreID());

    // BL0906 数据缓冲区 (3 字节)
    uint8_t bl_data_buffer[3];

    for (;;)
    {
        Serial.println("功率监控任务启动");
        for (uint8_t relay_num = 1; relay_num <= 3; relay_num++)
        {
            // 检查继电器是否激活 (ON)
            if (ELECTRIC_RELAY_get_state(relay_num) == 1)
            {
                Serial.printf("功率监控任务 -> 处理吸合的继电器 -> %d \n", relay_num);
                uint8_t relay_idx = relay_num - 1;

                // 1. 从 BL0906 读取电流
                if (BL_read_register(CURRENT_REGISTERS[relay_idx], bl_data_buffer))
                {
                    // bl_data_buffer[0] = LSB, bl_data_buffer[1] = MID, bl_data_buffer[2] = MSB
                    float current_reg = ((uint32_t)bl_data_buffer[2] << 16) | ((uint32_t)bl_data_buffer[1] << 8) | bl_data_buffer[0];
                    // 转换为实际电流 (A)
                    float current = BL_currentRegister2ActualCurrent(current_reg);

                    // 通过 HPLC 发送原始电流数据
                    uint8_t currentFrame[] = {
                        0x14,              // 控制码
                        0x0A,              // 数据域长度
                        LOCAL_ADDRESS[0],  // 本机地址
                        LOCAL_ADDRESS[1],  // 本机地址
                        LOCAL_ADDRESS[2],  // 本机地址
                        LOCAL_ADDRESS[3],  // 本机地址
                        LOCAL_ADDRESS[4],  // 本机地址
                        LOCAL_ADDRESS[5],  // 本机地址
                        relay_num,         // 插孔ID
                        bl_data_buffer[0], // 电流低字节
                        bl_data_buffer[1], // 电流中字节
                        bl_data_buffer[2]  // 电流高字节
                    };
                    if (electricParamPush)
                    {
                        // 获取HPLC互斥锁后再发送
                        if (xSemaphoreTake(hplcMutex, (TickType_t)10) == pdTRUE)
                        {
                            HPLC_send_frame(TARGET_ADDRESS, currentFrame, sizeof(currentFrame), false);
                            // 释放HPLC互斥锁
                            xSemaphoreGive(hplcMutex);
                        }
                    }
                    Serial.printf("SOCKET_ID -> %d | CURRENT -> %.3f\n", relay_num, current);
                }
                else
                {
                    Serial.printf("SOCKET_ID -> %d | CURRENT -> 读取失败\n", relay_num);
                }

                // 2. 从 BL0906 读取功率
                if (BL_read_register(POWER_REGISTERS[relay_idx], bl_data_buffer))
                {
                    // bl_data_buffer[0] = LSB, bl_data_buffer[1] = MID, bl_data_buffer[2] = MSB
                    uint32_t power_reg = ((uint32_t)bl_data_buffer[2] << 16) | ((uint32_t)bl_data_buffer[1] << 8) | bl_data_buffer[0];
                    // 转换为实际功率 (W)
                    float power = BL_powerRegister2ActualPower(power_reg);

                    // 通过 HPLC 发送原始功率数据
                    uint8_t powerFrame[] = {
                        0x15,              // 控制码
                        0x0A,              // 数据域长度
                        LOCAL_ADDRESS[0],  // 本机地址
                        LOCAL_ADDRESS[1],  // 本机地址
                        LOCAL_ADDRESS[2],  // 本机地址
                        LOCAL_ADDRESS[3],  // 本机地址
                        LOCAL_ADDRESS[4],  // 本机地址
                        LOCAL_ADDRESS[5],  // 本机地址
                        relay_num,         // 插孔ID
                        bl_data_buffer[0], // 功率低字节
                        bl_data_buffer[1], // 功率中字节
                        bl_data_buffer[2]  // 功率高字节
                    };
                    if (electricParamPush)
                    {
                        // 获取HPLC互斥锁后再发送
                        if (xSemaphoreTake(hplcMutex, (TickType_t)10) == pdTRUE)
                        {
                            HPLC_send_frame(TARGET_ADDRESS, powerFrame, sizeof(powerFrame), false);
                            // 释放HPLC互斥锁
                            xSemaphoreGive(hplcMutex);
                        }
                    }
                    Serial.printf("SOCKET_ID -> %d | POWER -> %.3f\n", relay_num, power);

                    // 3. 检查功率限制
                    // 读取最大功率
                    uint16_t max_power = ELECTRIC_RELAY_get_max_power(relay_num);
                    if (max_power > 0 && power > (float)max_power)
                    {
                        // 4. 超功率，关闭继电器
                        ELECTRIC_RELAY_control(relay_num, 0);

                        // 通过 HPLC 发送超功率事件
                        uint8_t powerExceedFrame[] = {
                            0x13,             // 控制码
                            0x07,             // 数据域长度
                            LOCAL_ADDRESS[0], // 本机地址
                            LOCAL_ADDRESS[1], // 本机地址
                            LOCAL_ADDRESS[2], // 本机地址
                            LOCAL_ADDRESS[3], // 本机地址
                            LOCAL_ADDRESS[4], // 本机地址
                            LOCAL_ADDRESS[5], // 本机地址
                            relay_num         // 插孔ID
                        };
                        // 获取HPLC互斥锁后再发送
                        if (xSemaphoreTake(hplcMutex, (TickType_t)10) == pdTRUE)
                        {
                            HPLC_send_frame(TARGET_ADDRESS, powerExceedFrame, sizeof(powerExceedFrame), true);
                            // 释放HPLC互斥锁
                            xSemaphoreGive(hplcMutex);
                        }
                        Serial.printf("SOCKET_ID -> %d | POWER_EXCEED -> %.3f > %d\n", relay_num, power, max_power);
                    }
                }
                else
                {
                    Serial.printf("SOCKET_ID -> %d | POWER -> 读取失败\n", relay_num);
                }
            }
        }
        Serial.println("功率监控任务结束");

        // 等待下一个监控周期
        vTaskDelay(pdMS_TO_TICKS(POWER_MONITOR_INTERVAL_MS));
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

    case 0x11:
    {
        // 接收CCO设置插孔开关状态
        Serial.println("接收CCO设置插孔开关状态");
        // 提取插孔ID和开关状态
        uint8_t socketId = frameParser.buffer[7];
        uint8_t socketState = frameParser.buffer[8];
        // 控制对应插孔的开关状态并持久化
        ELECTRIC_RELAY_control(socketId, socketState);

        // 响应ACK帧
        ackFrame[0] = 0x91;
        // 发送ACK帧
        HPLC_send_frame(TARGET_ADDRESS, ackFrame, sizeof(ackFrame), false);
        Serial.printf("SOCKET_ID -> %d | STATE -> %s\n", socketId, socketState == 0x01 ? "ON" : "OFF");
        break;
    }

    case 0x12:
    {
        // 接收CCO设置插孔最大功率
        Serial.println("接收CCO设置插孔最大功率");
        // 提取插孔ID和最大功率
        uint8_t socketId = frameParser.buffer[7];
        uint8_t powerLowByte = frameParser.buffer[8];
        uint8_t powerHighByte = frameParser.buffer[9];
        uint16_t maxPower = (powerHighByte << 8) | powerLowByte;
        // 设置对应插孔的最大功率并持久化
        ELECTRIC_RELAY_set_max_power(socketId, maxPower);

        // 响应ACK帧
        ackFrame[0] = 0x92;
        // 发送ACK帧
        HPLC_send_frame(TARGET_ADDRESS, ackFrame, sizeof(ackFrame), false);
        Serial.printf("SOCKET_ID -> %d | MAX_POWER -> %d\n", socketId, maxPower);
        break;
    }

    case 0x13:
    {
        // 接收CCO设置推送开关
        Serial.println("接收CCO设置推送开关");
        // 更新开关状态
        electricParamPush = frameParser.buffer[7] == 0x01;

        // 响应ACK帧
        ackFrame[0] = 0x93;
        // 发送ACK帧
        HPLC_send_frame(TARGET_ADDRESS, ackFrame, sizeof(ackFrame), false);
        Serial.printf("PUSH -> %d\n", electricParamPush);
        break;
    }

    default:
        break;
    }
}