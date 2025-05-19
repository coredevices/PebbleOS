/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

// ----------------------------------------------
//  Board definitions for Snowy EVT2 and similar
// ----------------------------------------------
//
// This includes snowy_evt2, snowy_dvt and snowy_bb2. Except for a couple of
// minor quirks, all of the boards using this file are electrically identical.

#include "drivers/imu/bmi160/bmi160.h"
#include "services/imu/units.h"
#include "util/size.h"

#define BT_VENDOR_ID 0x0154
#define BT_VENDOR_NAME "Pebble Technology"

#define BOARD_LSE_MODE RCC_LSE_Bypass

static const BoardConfig BOARD_CONFIG = {
  .has_mic = true,
  .mic_config = {
    .i2s_ck = { GPIOB, GPIO_Pin_10, GPIO_PinSource10, GPIO_AF_SPI2 },
    .i2s_sd = { GPIOB, GPIO_Pin_15, GPIO_PinSource15, GPIO_AF_SPI2 },
    .spi = SPI2,
    .spi_clock_ctrl = RCC_APB1Periph_SPI2,
#ifdef IS_BIGBOARD
    .gain = 100,
#else
    .gain = 250,
#endif
  },

#ifdef BOARD_SNOWY_S3
  .ambient_light_dark_threshold = 3220,
#else
  .ambient_light_dark_threshold = 3130,
#endif
  .ambient_k_delta_threshold = 96,
  .photo_en = { GPIOA, GPIO_Pin_3, true },

  .dbgserial_int = { EXTI_PortSourceGPIOC, 12 },
  .dbgserial_int_gpio = { GPIOC, GPIO_Pin_12 },

  // Only used with Sharp displays
  .lcd_com = { 0 },

  .power_5v0_options = OptionNotPresent,
  .power_ctl_5v0 = { 0 },

  .backlight_on_percent = 45,
  .backlight_max_duty_cycle_percent = 100,

  .fpc_pinstrap_1 = { GPIOB, GPIO_Pin_0 },
  .fpc_pinstrap_2 = { GPIOC, GPIO_Pin_5 },

#ifdef IS_BIGBOARD
  .num_avail_gpios = 140,
#else
  .num_avail_gpios = 114,
#endif
};

static const BoardConfigButton BOARD_CONFIG_BUTTON = {
  .buttons = {
    [BUTTON_ID_BACK]    = { "Back",   GPIOG, GPIO_Pin_4, { EXTI_PortSourceGPIOG, 4 }, GPIO_PuPd_NOPULL },
    [BUTTON_ID_UP]      = { "Up",     GPIOG, GPIO_Pin_3, { EXTI_PortSourceGPIOG, 3 }, GPIO_PuPd_UP },
    [BUTTON_ID_SELECT]  = { "Select", GPIOG, GPIO_Pin_1, { EXTI_PortSourceGPIOG, 1 }, GPIO_PuPd_UP },
    [BUTTON_ID_DOWN]    = { "Down",   GPIOG, GPIO_Pin_2, { EXTI_PortSourceGPIOG, 2 }, GPIO_PuPd_UP },
  },

  .button_com = { 0 },
  .active_high = false,
};

static const BoardConfigPower BOARD_CONFIG_POWER = {
  .pmic_int = { EXTI_PortSourceGPIOG, 7 },
  .pmic_int_gpio = {
    .gpio = GPIOG,
    .gpio_pin = GPIO_Pin_7,
  },

  .rail_4V5_ctrl = {
    .gpio = GPIOF,
    .gpio_pin = GPIO_Pin_2,
    .active_high = true,
  },
  .rail_6V6_ctrl = {
    .gpio = GPIOF,
    .gpio_pin = GPIO_Pin_3,
    .active_high = true,
  },
  .rail_6V6_ctrl_otype = GPIO_OType_OD,

  .battery_vmon_scale = {
    // The PMIC divides the battery voltage by a ratio of 3:1 in order to move it down to a voltage
    // our ADC is capable of reading. The battery voltage varies around 4V~ and we're only capable
    // of reading up to 1.8V in the ADC.
    .numerator = 3,
    .denominator = 1,
  },

  .vusb_stat = { .gpio = GPIO_Port_NULL, },
  .chg_stat = { GPIO_Port_NULL },
  .chg_fast = { GPIO_Port_NULL },
  .chg_en = { GPIO_Port_NULL },
  .has_vusb_interrupt = false,

  .wake_on_usb_power = false,

#if defined(IS_BIGBOARD) && !defined(BATTERY_DEBUG)
  .charging_cutoff_voltage = 4200,
#else
  .charging_cutoff_voltage = 4300,
#endif
  .charging_status_led_voltage_compensation = 0,

#ifdef BOARD_SNOWY_S3
  .low_power_threshold = 2,
  .battery_capacity_hours = 204,
#else
  .low_power_threshold = 3,
  .battery_capacity_hours = 144,
#endif
};

static const BoardConfigAccel BOARD_CONFIG_ACCEL = {
  .accel_config = {
#ifdef IS_BIGBOARD
    .axes_offsets[AXIS_X] = 0,
    .axes_offsets[AXIS_Y] = 1,
    .axes_offsets[AXIS_Z] = 2,
    .axes_inverts[AXIS_X] = false,
    .axes_inverts[AXIS_Y] = false,
    .axes_inverts[AXIS_Z] = true,
#else
    .axes_offsets[AXIS_X] = 1,
    .axes_offsets[AXIS_Y] = 0,
    .axes_offsets[AXIS_Z] = 2,
    .axes_inverts[AXIS_X] = false,
    .axes_inverts[AXIS_Y] = false,
    .axes_inverts[AXIS_Z] = false,
#endif
    .shake_thresholds[AccelThresholdHigh] = 0x64,
    .shake_thresholds[AccelThresholdLow] = 0xf,
    .double_tap_threshold = 12500,
  },
  .accel_int_gpios = {
    [0] = { GPIOG, GPIO_Pin_5 },
    [1] = { GPIOG, GPIO_Pin_6 },
  },
  .accel_ints = {
    [0] = { EXTI_PortSourceGPIOG, 5 },
    [1] = { EXTI_PortSourceGPIOG, 6 }
  },
};

static const BoardConfigMag BOARD_CONFIG_MAG = {
  .mag_config = {
#ifdef IS_BIGBOARD
    .axes_offsets[AXIS_X] = 1,
    .axes_offsets[AXIS_Y] = 0,
    .axes_offsets[AXIS_Z] = 2,
    .axes_inverts[AXIS_X] = false,
    .axes_inverts[AXIS_Y] = true,
    .axes_inverts[AXIS_Z] = true,
#else
    .axes_offsets[AXIS_X] = 1,
    .axes_offsets[AXIS_Y] = 0,
    .axes_offsets[AXIS_Z] = 2,
    .axes_inverts[AXIS_X] = true,
    .axes_inverts[AXIS_Y] = true,
    .axes_inverts[AXIS_Z] = false,
#endif
  },
  .mag_int_gpio = { GPIOF, GPIO_Pin_14 },
  .mag_int = { EXTI_PortSourceGPIOF, 14 },
};

static const BoardConfigActuator BOARD_CONFIG_VIBE = {
  .options = ActuatorOptions_Ctl | ActuatorOptions_Pwm | ActuatorOptions_HBridge,
  .ctl = { GPIOF, GPIO_Pin_4, true },
  .pwm = {
    .output = { GPIOB, GPIO_Pin_8, true },
    .timer = {
      .peripheral = TIM10,
      .config_clock = RCC_APB2Periph_TIM10,
      .init = TIM_OC1Init,
      .preload = TIM_OC1PreloadConfig
    },
    .afcfg = { GPIOB, GPIO_Pin_8, GPIO_PinSource8, GPIO_AF_TIM10 },
  },
};

static const BoardConfigActuator BOARD_CONFIG_BACKLIGHT = {
  .options = ActuatorOptions_Pwm,
  .ctl = {0},
  .pwm = {
    .output = { GPIOB, GPIO_Pin_14, true },
    .timer = {
      .peripheral = TIM12,
      .config_clock = RCC_APB1Periph_TIM12,
      .init = TIM_OC1Init,
      .preload = TIM_OC1PreloadConfig
    },
    .afcfg = { GPIOB, GPIO_Pin_14, GPIO_PinSource14, GPIO_AF_TIM12 },
  },
};

static const BoardConfigAccessory BOARD_CONFIG_ACCESSORY = {
  .power_en = { GPIOF, GPIO_Pin_13, true },
  .int_gpio = { GPIOE, GPIO_Pin_0 },
  .exti = { EXTI_PortSourceGPIOE, 0 },
};

#define BOARD_BT_USART_IRQ_HANDLER USART1_IRQHandler
static const BoardConfigBTCommon BOARD_CONFIG_BT_COMMON = {
  .controller = CC2564B,
  .shutdown = { GPIOB, GPIO_Pin_12, false },
  .wakeup = {
    .int_gpio = { GPIOA, GPIO_Pin_11 },
    .int_exti = { EXTI_PortSourceGPIOA, 11 },
  },
};

static const BoardConfigMCO1 BOARD_CONFIG_MCO1 = {
  .output_enabled = true,
  .af_cfg = {
    .gpio = GPIOA,
    .gpio_pin = GPIO_Pin_8,
    .gpio_pin_source = GPIO_PinSource8,
    .gpio_af = GPIO_AF_MCO,
  },
  .an_cfg = {
    .gpio = GPIOA,
    .gpio_pin = GPIO_Pin_8,
  },
};

extern DMARequest * const COMPOSITOR_DMA;
extern DMARequest * const MIC_I2S_RX_DMA;

extern UARTDevice * const QEMU_UART;
extern UARTDevice * const DBG_UART;
extern UARTDevice * const ACCESSORY_UART;
extern UARTDevice * const BLUETOOTH_UART;

extern SPISlavePort * const BMI160_SPI;

extern I2CSlavePort * const I2C_MAG3110;
extern I2CSlavePort * const I2C_MAX14690;
extern I2CSlavePort * const I2C_MFI;

extern VoltageMonitorDevice * const VOLTAGE_MONITOR_ALS;
extern VoltageMonitorDevice * const VOLTAGE_MONITOR_BATTERY;

extern const TemperatureSensor * const TEMPERATURE_SENSOR;

static MicDevice * const MIC = (void *)0;

extern ICE40LPDevice * const ICE40LP;
