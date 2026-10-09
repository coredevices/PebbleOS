/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "display.h"

#include <stdbool.h>
#include <stdint.h>

#include <pbl/drivers/button_id.h>
#include <pbl/input/input.h>

typedef void GPIO_TypeDef;

#define GPIO_Port_NULL NULL
#define GPIO_Pin_NULL  0U

typedef enum {
  GPIO_OType_PP,
  GPIO_OType_OD,
} GPIOOType_TypeDef;

typedef enum {
  GPIO_PuPd_NOPULL,
  GPIO_PuPd_UP,
  GPIO_PuPd_DOWN,
} GPIOPuPd_TypeDef;

typedef struct {
  void *gpio;
  uint8_t gpio_pin;
} InputConfig;

typedef struct {
  void *gpio;
  uint8_t gpio_pin;
  bool active_high;
} OutputConfig;

typedef struct {
  uint8_t backlight_on_percent;
  uint32_t ambient_light_dark_threshold;
  uint32_t ambient_k_delta_threshold;
  uint32_t ambient_light_lux_dark_offset;
  uint32_t ambient_light_lux_num;
  uint32_t ambient_light_lux_den;
#ifdef CONFIG_BACKLIGHT_HAS_COLOR
  uint32_t backlight_default_color;
#endif
} BoardConfig;

typedef struct {
  const char *name;
  uint16_t code;
} ButtonConfig;

typedef struct {
  ButtonConfig buttons[NUM_BUTTONS];
} BoardConfigButton;

typedef struct {
  const uint8_t low_power_threshold;
  const uint16_t battery_capacity_hours;
} BoardConfigPower;

typedef struct {
  uint8_t default_motion_sensitivity;
} BoardConfigAccel;

typedef struct {
  int axes_offsets[3];
  bool axes_inverts[3];
} MagConfig;

typedef struct {
  const MagConfig mag_config;
} BoardConfigMag;

typedef const struct UARTDevice UARTDevice;
typedef const struct I2CBus I2CBus;
typedef const struct I2CSlavePort I2CSlavePort;
typedef const struct QSPIPort QSPIPort;
typedef const struct QSPIFlash QSPIFlash;
typedef const struct HRMDevice HRMDevice;
typedef const struct MicDevice MicDevice;
typedef const struct AudioDevice AudioDevice;

typedef struct PosixDisplayDevice {
  uint16_t width;
  uint16_t height;
} PosixDisplayDevice;
typedef const PosixDisplayDevice DisplayDevice;

void board_early_init(void);
void board_init(void);

#include "board_definitions.h"
