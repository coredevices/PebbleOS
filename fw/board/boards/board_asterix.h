#pragma once

#include <pbl/services/imu/units.h>

#define BT_VENDOR_ID   0x0EEA
#define BT_VENDOR_NAME "Core Devices LLC"

#define BOARD_LSE_MODE RCC_LSE_Bypass

#define BOARD_RTC_INST NRF_RTC1
#define BOARD_RTC_IRQN RTC1_IRQn

static const BoardConfig BOARD_CONFIG = {
  .ambient_light_dark_threshold = 100,
  .ambient_k_delta_threshold = 30,

  .backlight_on_percent = 25,

  .mic_config = {
    .gain = 40,
  }
};

static const BoardConfigPower BOARD_CONFIG_POWER = {
  .low_power_threshold = 2,
  .battery_capacity_hours = 450,
};

static const BoardConfigAccel BOARD_CONFIG_ACCEL = {
  .default_motion_sensitivity = 55U, // Medium
};

extern const struct pbl_uart *const DBG_UART;
extern QSPIPort *const QSPI;
extern QSPIFlash *const QSPI_FLASH;

extern MicDevice *const MIC;
extern AudioDevice *const AUDIO;

extern const struct pbl_i2c_dev *const I2C_DA7212;
