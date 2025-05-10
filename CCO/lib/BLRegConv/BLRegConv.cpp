#include <BLRegConv.h>

// 为电流转换预先计算的系数
// 公式: Vref / (12875.0 * GAIN_I * RL_mOhm)
const double CURRENT_CONVERSION_COEFFICIENT = BL0906_VREF / (12875.0 * BL0906_GAIN_I * BL0906_RL_MOHM);

// 为功率转换预先计算的系数
// 公式: (Vref^2 * (Rf+Rv)_kOhm) / (40.4125 * RL_mOhm * Gain_I * RV_kOhm * Gain_V * 1000.0)
const double POWER_CONVERSION_COEFFICIENT = (BL0906_VREF * BL0906_VREF * BL0906_RF_PLUS_RV_KOHM) / (40.4125 * BL0906_RL_MOHM * BL0906_GAIN_I * BL0906_RV_KOHM * BL0906_GAIN_V * 1000.0);

/**
 * @brief 将 24 位无符号电流寄存器值转换为实际电流 (A)
 * @param current_reg_val 从电流 RMS 寄存器读取的 24 位无符号值
 * @return float 计算得到的实际电流 (A)
 */
float BL_currentRegister2ActualCurrent(uint32_t current_reg_val)
{
    // 电流寄存器是 24 位无符号数。确保仅使用低 24 位。
    uint32_t val = current_reg_val & 0x00FFFFFF;
    return (float)(val * CURRENT_CONVERSION_COEFFICIENT);
}

/**
 * @brief 将 24 位有符号 (补码) 有功功率寄存器值转换为实际有功功率 (W)
 * @param power_reg_val 从有功功率寄存器读取的 24 位值，补码，Bit 23 是符号位
 * @return float 计算得到的实际有功功率 (W)
 */
float BL_powerRegister2ActualPower(uint32_t power_reg_val)
{
    // 有功功率寄存器是 24 位有符号数，补码表示。Bit 23 是符号位。
    int32_t signed_power_reg_val;

    // 考虑原始值的低 24 位
    uint32_t raw_val_24bit = power_reg_val & 0x00FFFFFF;

    // 检查第 24 位 (符号位，索引为 23) 是否被设置
    if (raw_val_24bit & 0x00800000)
    {
        // 负值，执行从 24 位到 32 位的符号扩展
        signed_power_reg_val = raw_val_24bit | 0xFF000000;
    }
    else
    {
        // 正值 (或零)
        signed_power_reg_val = raw_val_24bit;
    }

    // 返回绝对值
    return fabsf((float)(signed_power_reg_val * POWER_CONVERSION_COEFFICIENT));
}