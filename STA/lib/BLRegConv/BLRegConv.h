#ifndef BL_REG_CONV_H
#define BL_REG_CONV_H

#include <Arduino.h>

// Vref: 芯片参考电压(V)
const double BL0906_VREF = 1.097;

// GAIN_I: 电流通道增益
const double BL0906_GAIN_I = 16.0;

// GAIN_V: 电压通道增益
const double BL0906_GAIN_V = 1.0;

// RL_MOHM: 电流采样电阻值(毫欧)
const double BL0906_RL_MOHM = 1.0;

// RF_KOHM: Rf分压电阻(千欧)
const double BL0906_RF_KOHM = 300.0 * 5;

// RV_KOHM: Rv分压电阻(千欧)
const double BL0906_RV_KOHM = 1.0;

// RF_PLUS_RV_KOHM: 电压分压电阻之和(Rf + Rv)(千欧)
const double BL0906_RF_PLUS_RV_KOHM = BL0906_RF_KOHM + BL0906_RV_KOHM;

/**
 * @brief 将 24 位无符号电流寄存器值转换为实际电流 (A)
 * @param current_reg_val 从电流 RMS 寄存器读取的 24 位无符号值
 * @return float 计算得到的实际电流 (A)
 */
float BL_currentRegister2ActualCurrent(uint32_t current_reg_val);

/**
 * @brief 将 24 位有符号 (补码) 有功功率寄存器值转换为实际有功功率 (W)
 * @param power_reg_val 从有功功率寄存器读取的 24 位值，补码，Bit 23 是符号位
 * @return float 计算得到的实际有功功率 (W)
 */
float BL_powerRegister2ActualPower(uint32_t power_reg_val);

#endif