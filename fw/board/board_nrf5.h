/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// clang-format off
#include "display.h"

#include <stdint.h>

#include <pbl/drivers/button_id.h>
#include <pbl/input/input.h>
#include <pbl/drivers/gpio/nrf5.h>

#include <hal/nrf_gpio.h>
#include <nrfx_pdm.h>
#include <nrfx_spim.h>
#include <nrfx_timer.h>
// clang-format on

#define GPIO_Port_NULL (nullptr)
#define GPIO_Pin_NULL  ((uint16_t)-1)

typedef struct {
  const char *name;
  struct pbl_gpio gpio;
  uint16_t code;
} ButtonConfig;

typedef struct {
  struct pbl_gpio i2s_ck;
  struct pbl_gpio i2s_sd;
  NRF_SPIM_Type *spi;
  uint32_t spi_clock_ctrl;
  nrf_pdm_gain_t gain;
} MicConfig;

typedef struct {
  // Audio Configuration
  /////////////////////////////////////////////////////////////////////////////
  const MicConfig mic_config;

  // Ambient Light Configuration
  /////////////////////////////////////////////////////////////////////////////
  const uint32_t ambient_light_dark_threshold;
  const uint32_t ambient_k_delta_threshold;
  const struct pbl_gpio photo_en;

  const uint8_t backlight_on_percent; // percent of max possible brightness
} BoardConfig;

// Button Configuration
/////////////////////////////////////////////////////////////////////////////
typedef struct {
  const ButtonConfig buttons[NUM_BUTTONS];
  nrfx_timer_t timer;
} BoardConfigButton;

// Power Configuration
/////////////////////////////////////////////////////////////////////////////
typedef struct {
  //! Percentage for watch only mode
  const uint8_t low_power_threshold;

  //! Approximate hours of battery life
  const uint16_t battery_capacity_hours;
} BoardConfigPower;

typedef struct {
  uint8_t default_motion_sensitivity;
} BoardConfigAccel;

typedef struct {
  NRF_RTC_Type *rtc;
  NRF_GPIOTE_Type *gpiote;
  uint32_t psel;
  uint32_t period_us;
  uint32_t pulse_us;
} NrfLowPowerPWM;

typedef struct {
  nrfx_spim_t spi;

  const struct pbl_gpio mosi;
  const struct pbl_gpio clk;
  const struct pbl_gpio cs;

  const struct pbl_gpio on_ctrl;

  const NrfLowPowerPWM extcomin;
} BoardConfigSharpDisplay;

typedef const struct UARTDevice UARTDevice;
typedef const struct HRMDevice HRMDevice;
typedef const struct MicDevice MicDevice;
typedef const struct QSPIPort QSPIPort;
typedef const struct QSPIFlash QSPIFlash;
typedef const struct AudioDevice AudioDevice;

void board_early_init(void);
void board_init(void);

// clang-format off
#include <pbl/drivers/i2c/nrf5.h>
#include "board_definitions.h"
// clang-format on
