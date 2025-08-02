#pragma once

#include "drivers/pmic/npm1300.h"
#include "services/imu/units.h"
#include "util/size.h"

#define BT_VENDOR_ID 0x0EEA
#define BT_VENDOR_NAME "Core Devices LLC"

#define BOARD_LSE_MODE RCC_LSE_Bypass

#define BOARD_RTC_INST NRF_RTC1
#define BOARD_RTC_IRQN RTC1_IRQn

static const BoardConfig BOARD_CONFIG = {
  .ambient_light_dark_threshold = 100,
  .ambient_k_delta_threshold = 30,
  .als_always_on = true,

  .backlight_on_percent = 25,
  .backlight_max_duty_cycle_percent = 67,
  
  .dbgserial_int = {
    .peripheral = NRFX_GPIOTE_INSTANCE(0), 
    .channel = 0,
    .gpio_pin = NRF_GPIO_PIN_MAP(0, 5),
  },

  .has_mic = true,
};

static const BoardConfigButton BOARD_CONFIG_BUTTON = {
  .buttons = {
    [BUTTON_ID_BACK] =
        { "Back",   { NRFX_GPIOTE_INSTANCE(0), 2, NRF_GPIO_PIN_MAP(0, 28) }, NRF_GPIO_PIN_PULLUP },
    [BUTTON_ID_UP] =
        { "Up",     { NRFX_GPIOTE_INSTANCE(0), 3, NRF_GPIO_PIN_MAP(0, 29) }, NRF_GPIO_PIN_PULLUP },
    [BUTTON_ID_SELECT] =
        { "Select", { NRFX_GPIOTE_INSTANCE(0), 4, NRF_GPIO_PIN_MAP(0, 30) }, NRF_GPIO_PIN_PULLUP },
    [BUTTON_ID_DOWN] =
        { "Down",   { NRFX_GPIOTE_INSTANCE(0), 5, NRF_GPIO_PIN_MAP(0, 31) }, NRF_GPIO_PIN_PULLUP },
  },
  .active_high = false,
  .timer = NRFX_TIMER_INSTANCE(1),
};

static const BoardConfigPower BOARD_CONFIG_POWER = {
  .pmic_int = { NRFX_GPIOTE_INSTANCE(0), 1, NRF_GPIO_PIN_MAP(1, 12) },
  .pmic_int_gpio = { NRF5_GPIO_RESOURCE_EXISTS, NRF_GPIO_PIN_MAP(1, 12) },
  .low_power_threshold = 5,
  // Current is not great but getting there.  1 mA or so on 130 mAh battery;
  // Memfault reports 160h expected battery, but we'll conservatively
  // estimate 130 hours
  .battery_capacity_hours = 130,
};

static const BoardConfigActuator BOARD_CONFIG_VIBE = {
  .ctl = { NRF5_GPIO_RESOURCE_EXISTS, NRF_GPIO_PIN_MAP(0, 2), true }, // LRA_EN
  .vsys_scale = 3300,
};

static const BoardConfigAccel BOARD_CONFIG_ACCEL = {
  .accel_config = {
    .axes_offsets[AXIS_X] = 1,
    .axes_offsets[AXIS_Y] = 0,
    .axes_offsets[AXIS_Z] = 2,
    .axes_inverts[AXIS_X] = false,
    .axes_inverts[AXIS_Y] = false,
    .axes_inverts[AXIS_Z] = false,
    // This will need calibration.
    .shake_thresholds[AccelThresholdHigh] = 64,
    .shake_thresholds[AccelThresholdLow] = 0xf,
    .double_tap_threshold = 12500,
  },
  // Ideally we would configure both interrupt pins, but we have run out of GPIOTE channels.
  // We will use INT1 (connected to pin 13) for accelerometer interrupts, and leave INT2 (pin 11) unused.
  .accel_int_gpios = {
    [0] = { .gpio = NRF5_GPIO_RESOURCE_EXISTS, .gpio_pin = NRF_GPIO_PIN_MAP(1, 13) },
  },
  .accel_ints = {
    [0] = { .peripheral = NRFX_GPIOTE_INSTANCE(0), .channel = 7, .gpio_pin = NRF_GPIO_PIN_MAP(1, 13) },
  },
};

extern UARTDevice * const DBG_UART;

extern PwmState BACKLIGHT_PWM_STATE;
static const BoardConfigActuator BOARD_CONFIG_BACKLIGHT = {
  .options = ActuatorOptions_Pwm | ActuatorOptions_Ctl,
  .ctl = { NRF5_GPIO_RESOURCE_EXISTS, NRF_GPIO_PIN_MAP(1, 8), true },
  .pwm = {
    .state = &BACKLIGHT_PWM_STATE,
    .output = { NRF5_GPIO_RESOURCE_EXISTS, NRF_GPIO_PIN_MAP(0, 26), true },
    .peripheral = NRFX_PWM_INSTANCE(0)
  },
};

extern PwmState DISPLAY_EXTCOMIN_STATE;
static const BoardConfigSharpDisplay BOARD_CONFIG_DISPLAY = {
  .spi = NRFX_SPIM_INSTANCE(3),

  .clk = { NRF5_GPIO_RESOURCE_EXISTS, NRF_GPIO_PIN_MAP(0, 6), true },
  .mosi = { NRF5_GPIO_RESOURCE_EXISTS, NRF_GPIO_PIN_MAP(0, 8), true },
  .cs = { NRF5_GPIO_RESOURCE_EXISTS, NRF_GPIO_PIN_MAP(1, 3), true },

  .on_ctrl = { NRF5_GPIO_RESOURCE_EXISTS, NRF_GPIO_PIN_MAP(0, 4), true },

  .extcomin = {
    .state = &DISPLAY_EXTCOMIN_STATE,
    .output = { NRF5_GPIO_RESOURCE_EXISTS, NRF_GPIO_PIN_MAP(1, 15), true },
    .peripheral = NRFX_PWM_INSTANCE(1),
  },
};

extern QSPIPort * const QSPI;
extern QSPIFlash * const QSPI_FLASH;

extern MicDevice * const MIC;

extern I2CSlavePort * const I2C_NPM1300;
extern I2CSlavePort * const I2C_DRV2604;
extern I2CSlavePort * const I2C_OPT3001;
extern I2CSlavePort * const I2C_DA7212;
extern I2CSlavePort * const I2C_MMC5603NJ;
extern I2CSlavePort * const I2C_BMP390;
extern I2CSlavePort * const I2C_LSM6D;

extern const Npm1300Config NPM1300_CONFIG;
