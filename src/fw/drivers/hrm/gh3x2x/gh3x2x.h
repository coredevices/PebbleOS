/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once
#include <stdbool.h>
#include "drivers/exti.h"
#include "drivers/gpio.h"
#include "drivers/i2c.h"
#include "drivers/i2c_definitions.h"
#include "applib/app_timer.h"

#if PLATFORM_OBELIX
// FIXME(OBELIX): Provide proper GPIO layer abstraction
#define GH3X2X_RESET_PIN_CTRLBY_NPM1300  1
#endif

#define HRM_PPG_CH_NUM                      6
#define HRM_PPG_FACTORY_TEST_FIFO_LEN       80
typedef struct {
  double result[HRM_PPG_CH_NUM];
  uint16_t test_mode;
  uint16_t drop_count;
  uint16_t wpos;
  uint16_t count;
  uint32_t *ppg_array[HRM_PPG_CH_NUM];
} GH3x2xFTData;

typedef struct HRMDeviceState {
  bool enabled;
  bool initialized;
  bool is_wear;
  uint16_t timer_period_ms;
  AppTimer *timer;
  GH3x2xFTData* factory;
} HRMDeviceState;

typedef const struct HRMDevice {
  HRMDeviceState *state;
  I2CSlavePort *i2c;
  ExtiConfig int_exti;
  OutputConfig reset_gpio;
} HRMDevice;


bool gh3x2x_ble_data_recv(void* context);
void gh3x2x_wear_evt_notify(bool is_wear);
void gh3x2x_rawdata_notify(uint32_t *p_rawdata, uint32_t data_count);

void gh3x2x_wear_evt_notify(bool is_wear);
bool gh3x2x_is_wear_get(void);

typedef enum {
  HRM_FACTORY_TEST_NONE,
  HRM_FACTORY_TEST_CTR,
  HRM_FACTORY_TEST_LIGHT_LEAK,
  HRM_FACTORY_TEST_HSM,
} GH3x2xFTType;

void gh3x2x_start_ft_ctr(void);
void gh3x2x_start_ft_leakage(void);

