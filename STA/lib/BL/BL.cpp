#include <BL.h>

/**
 * @brief 初始化BL0906电能计量芯片串口，并加载最大功率设置
 */
void BL_init()
{
    // 初始化串口
    BL.begin(19200, SERIAL_8N1, BL_RX, BL_TX);

    // 延时确保串口初始化完成
    delay(500);

    // 0x9E(USR_WRPROT)-用户写保护设置寄存器-允许写入｜0x5555
    uint8_t USR_WRPROT[3] = {0x55, 0x55, 0x00};
    if (BL_write_register(0x9E, USR_WRPROT))
    {
        Serial.println("USR_WRPROT寄存器设置成功");
    }
    else
    {
        Serial.println("USR_WRPROT寄存器设置失败");
    }
    // 0x93(ADC_PD)-ADC 关断寄存器-关闭456通道寄存器降低功耗｜0x07e2
    uint8_t ADC_PD[3] = {0xE2, 0x07, 0x00};
    if (BL_write_register(0x93, ADC_PD))
    {
        Serial.println("ADC_PD寄存器设置成功");
    }
    else
    {
        Serial.println("ADC_PD寄存器设置失败");
    }
    // 0x60(GAIN1)-通道 PGA 增益调整寄存器1-电流通道采用 16 倍增益，电压通道采用 1 倍增益｜0x333300
    uint8_t GAIN1[3] = {0x00, 0x33, 0x33};
    if (BL_write_register(0x60, GAIN1))
    {
        Serial.println("GAIN1寄存器设置成功");
    }
    else
    {
        Serial.println("GAIN1寄存器设置失败");
    }
    // 0x61(GAIN2)-通道 PGA 增益调整寄存器2-电流通道采用 16 倍增益，电压通道采用 1 倍增益｜0x003300
    uint8_t GAIN2[3] = {0x00, 0x33, 0x00};
    if (BL_write_register(0x61, GAIN2))
    {
        Serial.println("GAIN2寄存器设置成功");
    }
    else
    {
        Serial.println("GAIN2寄存器设置失败");
    }
}

/**
 * @brief 计算BL0906 UART通信的校验和
 * @param address 寄存器地址
 * @param data1 第一个数据字节
 * @param data2 第二个数据字节
 * @param data3 第三个数据字节
 * @return uint8_t 计算得到的校验和
 */
static uint8_t calculate_checksum(uint8_t address, uint8_t data1, uint8_t data2, uint8_t data3)
{
    return ~((address + data1 + data2 + data3) & 0xFF);
}

/**
 * @brief 从BL0906读取指定寄存器的3字节数据
 * @param address 要读取的寄存器地址
 * @param dataBuffer 指向用于存储读取数据的3字节数组的指针
 * @return bool 读取成功返回true，失败返回false
 */
bool BL_read_register(uint8_t address, uint8_t *dataBuffer)
{
    if (dataBuffer == nullptr)
    {
        return false;
    }

    // 清空串口接收缓冲区
    while (BL.available())
    {
        BL.read();
    }

    // 1. 发送读命令和地址
    BL.write(BL0906_READ_CMD);
    BL.write(address);
    // 等待发送完成
    BL.flush();

    // 2. 等待并读取BL0906返回的数据 (3字节数据 + 1字节校验和)
    uint8_t receivedBytes[4];
    unsigned long startTime = millis();
    int bytesRead = 0;
    while (bytesRead < 4 && (millis() - startTime < BL0906_READ_TIMEOUT))
    {
        if (BL.available())
        {
            receivedBytes[bytesRead++] = BL.read();
        }
    }

    if (bytesRead < 4)
    {
        // 超时或数据接收不完整
        return false;
    }

    // 3. 提取数据和校验和
    dataBuffer[0] = receivedBytes[0]; // DATA[7:0]
    dataBuffer[1] = receivedBytes[1]; // DATA[15:8]
    dataBuffer[2] = receivedBytes[2]; // DATA[23:16]
    uint8_t receivedChecksum = receivedBytes[3];

    // 4. 计算校验和并验证
    uint8_t calculatedChecksum = calculate_checksum(address, dataBuffer[0], dataBuffer[1], dataBuffer[2]);

    if (calculatedChecksum == receivedChecksum)
    {
        // 校验成功
        return true;
    }
    else
    {
        // 校验失败
        return false;
    }
}

/**
 * @brief 向BL0906指定寄存器写入3字节数据
 * @param address 要写入的寄存器地址
 * @param dataBuffer 指向包含要写入数据的3字节数组的指针
 * @return bool 发送成功返回true，失败返回false
 */
bool BL_write_register(uint8_t address, const uint8_t *dataBuffer)
{
    if (dataBuffer == nullptr)
    {
        return false;
    }

    // 1. 计算校验和
    uint8_t checksum = calculate_checksum(address, dataBuffer[0], dataBuffer[1], dataBuffer[2]);

    // 2. 发送写命令、地址、数据和校验和
    BL.write(BL0906_WRITE_CMD);
    BL.write(address);
    BL.write(dataBuffer[0]);
    BL.write(dataBuffer[1]);
    BL.write(dataBuffer[2]);
    BL.write(checksum);
    // 等待发送完成
    BL.flush();

    return true;
}