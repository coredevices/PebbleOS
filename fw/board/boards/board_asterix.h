#pragma once

#include <pbl/drivers/pwm/nrf5_rtc.h>
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

static const BoardConfigButton BOARD_CONFIG_BUTTON = {
  .buttons =
      {
        [BUTTON_ID_BACK] =
            {"Back", PBL_GPIO(NRF5_GPIO_P0, 28, PBL_GPIO_PULL_UP | PBL_GPIO_ACTIVE_LOW),
             PBL_INPUT_KEY_BACK},
        [BUTTON_ID_UP] =
            {"Up", PBL_GPIO(NRF5_GPIO_P0, 29, PBL_GPIO_PULL_UP | PBL_GPIO_ACTIVE_LOW),
             PBL_INPUT_KEY_UP},
        [BUTTON_ID_SELECT] =
            {"Select", PBL_GPIO(NRF5_GPIO_P0, 30, PBL_GPIO_PULL_UP | PBL_GPIO_ACTIVE_LOW),
             PBL_INPUT_KEY_SELECT},
        [BUTTON_ID_DOWN] =
            {"Down", PBL_GPIO(NRF5_GPIO_P0, 31, PBL_GPIO_PULL_UP | PBL_GPIO_ACTIVE_LOW),
             PBL_INPUT_KEY_DOWN},
      },
  .timer = NRFX_TIMER_INSTANCE(1),
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

static const BoardConfigSharpDisplay BOARD_CONFIG_DISPLAY = {
  .spi = NRFX_SPIM_INSTANCE(3),

  .clk = PBL_GPIO(NRF5_GPIO_P0, 6, 0),
  .mosi = PBL_GPIO(NRF5_GPIO_P0, 8, 0),
  .cs = PBL_GPIO(NRF5_GPIO_P1, 3, 0),

  .on_ctrl = PBL_GPIO(NRF5_GPIO_P0, 4, 0),

  // Feeds a flip-flop, generating a 60 Hz 50% signal to EXTCOMIN
  .extcomin = PBL_PWM(&EXTCOMIN_PWM.ctrl, 0, PBL_PWM_HZ(120), 0),
};

extern QSPIPort *const QSPI;
extern QSPIFlash *const QSPI_FLASH;

extern MicDevice *const MIC;
extern AudioDevice *const AUDIO;

extern const struct pbl_i2c_dev *const I2C_DA7212;
