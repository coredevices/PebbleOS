#pragma once

#include <pbl/drivers/pwm/nrf5_rtc.h>
#include <pbl/drivers/spi/nrf5.h>
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

extern UARTDevice *const DBG_UART;
extern const struct pbl_pwm_nrf5_rtc EXTCOMIN_PWM;
extern const struct pbl_spi_nrf5 DISPLAY_SPI;

static const BoardConfigSharpDisplay BOARD_CONFIG_DISPLAY = {
  .spi = PBL_SPI_DEV(&DISPLAY_SPI.bus, 0, 1000000,
                     PBL_SPI_MODE_0 | PBL_SPI_CS_HIGH | PBL_SPI_LSB_FIRST),

  .on_ctrl = PBL_GPIO(NRF5_GPIO_P0, 4, 0),

  // Feeds a flip-flop, generating a 60 Hz 50% signal to EXTCOMIN
  .extcomin = PBL_PWM(&EXTCOMIN_PWM.ctrl, 0, PBL_PWM_HZ(120), 0),
};

extern QSPIPort *const QSPI;
extern QSPIFlash *const QSPI_FLASH;

extern MicDevice *const MIC;
extern AudioDevice *const AUDIO;

extern const struct pbl_i2c_dev *const I2C_DA7212;
