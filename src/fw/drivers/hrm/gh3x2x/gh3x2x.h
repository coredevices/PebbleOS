/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>
#include "drivers/exti.h"
#include "drivers/gpio.h"
#include "drivers/i2c.h"
#include "drivers/i2c_definitions.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GH3X2X_RESET_PIN_CTRLBY_NPM1300  (1)

typedef struct HRMDeviceState {
  bool enabled;
} HRMDeviceState;

typedef const struct HRMDevice {
  HRMDeviceState *state;
  I2CSlavePort *i2c;
  ExtiConfig int_exti;
  OutputConfig reset_gpio;
} HRMDevice;


// @brief 初始化复位引脚
void gh3026_reset_pin_init(void);
// @brief 控制复位引脚电平高低
void gh3026_reset_pin_ctrl(uint8_t pin_level);
// @brief 初始化i2c
void gh3026_i2c_init(void);
// @brief  i2c 读数据
void gh3026_i2c_read(uint8_t device_id, const uint8_t write_buffer[], uint16_t write_length, uint8_t read_buffer[], uint16_t read_length);
// @brief i2c写数据
void gh3026_i2c_write(uint8_t device_id, const uint8_t write_buffer[], uint16_t length);
// @brief 中断引脚初始化
void gh3026_int_pin_init(void);
// @brief 喂gsensor数据
//void gh3026_gsensor_data_get(int16_t* p_gsensor_buffer, uint16_t *gsensor_buffer_index);
// @brief 上报测量值
void hrm_result_report(uint8_t type, uint32_t val, uint8_t quality);


#ifdef __cplusplus
}
#endif

