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
//  Board definitions for robert_bb, robert_bb2, robert_evt, cutts_bb
// ----------------------------------------------
//

#include "drivers/imu/bmi160/bmi160.h"
#include "services/imu/units.h"
#include "util/size.h"

#define BT_VENDOR_ID 0x0154
#define BT_VENDOR_NAME "Pebble Technology"

#define BOARD_LSE_MODE RCC_LSE_Bypass

static const BoardConfig BOARD_CONFIG = {
  .ambient_light_dark_threshold = 3220,
  .ambient_k_delta_threshold = 96,
  .photo_en = { GPIOF, GPIO_Pin_5, true },
  .als_always_on = true,

  .dbgserial_int = { EXTI_PortSourceGPIOH, 9 },
  .dbgserial_int_gpio = { GPIOH, GPIO_Pin_9 },

  // Only used with Sharp displays
  .lcd_com = { 0 },

  .power_5v0_options = OptionNotPresent,
  .power_ctl_5v0 = { 0 },

  .backlight_on_percent = 45,
  .backlight_max_duty_cycle_percent = 100,

  .num_avail_gpios = 140,

  .has_mic = true,
};

static const BoardConfigButton BOARD_CONFIG_BUTTON = {
  .buttons = {
#if BOARD_ROBERT_BB || BOARD_CUTTS_BB || BOARD_ROBERT_BB2
    [BUTTON_ID_BACK]    =
        { "Back",   GPIOG, GPIO_Pin_6, { EXTI_PortSourceGPIOG, 6 }, GPIO_PuPd_UP },
    [BUTTON_ID_UP]      =
        { "Up",     GPIOG, GPIO_Pin_3, { EXTI_PortSourceGPIOG, 3 }, GPIO_PuPd_NOPULL },
    [BUTTON_ID_DOWN]    =
        { "Down",   GPIOG, GPIO_Pin_4, { EXTI_PortSourceGPIOG, 4 }, GPIO_PuPd_UP },
#elif BOARD_ROBERT_EVT
    [BUTTON_ID_BACK]    =
        { "Back",   GPIOG, GPIO_Pin_3, { EXTI_PortSourceGPIOG, 3 }, GPIO_PuPd_NOPULL },
    [BUTTON_ID_UP]      =
        { "Up",     GPIOG, GPIO_Pin_4, { EXTI_PortSourceGPIOG, 4 }, GPIO_PuPd_UP },
    [BUTTON_ID_DOWN]    =
        { "Down",   GPIOG, GPIO_Pin_6, { EXTI_PortSourceGPIOG, 6 }, GPIO_PuPd_UP },
#else
#error "Unknown board"
#endif
    [BUTTON_ID_SELECT]  =
        { "Select", GPIOG, GPIO_Pin_5, { EXTI_PortSourceGPIOG, 5 }, GPIO_PuPd_UP },
  },

  .button_com = { 0 },
  .active_high = false,
};

static const BoardConfigPower BOARD_CONFIG_POWER = {
  .pmic_int = { EXTI_PortSourceGPIOF, 12 },
  .pmic_int_gpio = {
    .gpio = GPIOF,
    .gpio_pin = GPIO_Pin_12,
  },

  .rail_4V5_ctrl = {
    .gpio = GPIOH,
    .gpio_pin = GPIO_Pin_5,
    .active_high = true,
  },
#if BOARD_CUTTS_BB
  .rail_6V6_ctrl = {
    .gpio = GPIOH,
    .gpio_pin = GPIO_Pin_3,
    .active_high = true,
  },
  .rail_6V6_ctrl_otype = GPIO_OType_PP,
#elif BOARD_ROBERT_BB || BOARD_ROBERT_BB2 || BOARD_ROBERT_EVT
  .rail_6V6_ctrl = { GPIO_Port_NULL },
#else
#error "Unknown board"
#endif

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

  .low_power_threshold = 2,
  .battery_capacity_hours = 204,
};

static const BoardConfigAccel BOARD_CONFIG_ACCEL = {
  .accel_config = {
    .axes_offsets[AXIS_X] = 0,
    .axes_offsets[AXIS_Y] = 1,
    .axes_offsets[AXIS_Z] = 2,
#if BOARD_ROBERT_BB || BOARD_ROBERT_BB2 || BOARD_CUTTS_BB
    .axes_inverts[AXIS_X] = true,
    .axes_inverts[AXIS_Y] = false,
    .axes_inverts[AXIS_Z] = true,
#elif BOARD_ROBERT_EVT
    .axes_inverts[AXIS_X] = false,
    .axes_inverts[AXIS_Y] = true,
    .axes_inverts[AXIS_Z] = false,
#else
#error "Unknown board"
#endif
    .shake_thresholds[AccelThresholdHigh] = 0x64,
    .shake_thresholds[AccelThresholdLow] = 0xf,
    .double_tap_threshold = 12500,
  },
  .accel_int_gpios = {
    [0] = { GPIOH, GPIO_Pin_15 },
    [1] = { GPIOH, GPIO_Pin_14 },
  },
  .accel_ints = {
    [0] = { EXTI_PortSourceGPIOH, 15 },
    [1] = { EXTI_PortSourceGPIOH, 14 }
  },
};

static const BoardConfigMag BOARD_CONFIG_MAG = {
  .mag_config = {
    .axes_offsets[AXIS_X] = 1,
    .axes_offsets[AXIS_Y] = 0,
    .axes_offsets[AXIS_Z] = 2,
#if BOARD_ROBERT_BB || BOARD_ROBERT_BB2 || BOARD_CUTTS_BB
    .axes_inverts[AXIS_X] = false,
    .axes_inverts[AXIS_Y] = true,
    .axes_inverts[AXIS_Z] = true,
#elif BOARD_ROBERT_EVT
    .axes_inverts[AXIS_X] = true,
    .axes_inverts[AXIS_Y] = true,
    .axes_inverts[AXIS_Z] = false,
#else
#error "Unknown board"
#endif
  },
  .mag_int_gpio = { GPIOF, GPIO_Pin_11 },
  .mag_int = { EXTI_PortSourceGPIOF, 11 },
};

static const BoardConfigActuator BOARD_CONFIG_VIBE = {
  .options = ActuatorOptions_Ctl | ActuatorOptions_Pwm | ActuatorOptions_HBridge,
#if BOARD_ROBERT_BB || BOARD_CUTTS_BB
  .ctl = { GPIOD, GPIO_Pin_14, true },
  .pwm = {
    .output = { GPIOD, GPIO_Pin_12, true },
    .timer = {
      .peripheral = TIM4,
      .config_clock = RCC_APB1Periph_TIM4,
      .init = TIM_OC1Init,
      .preload = TIM_OC1PreloadConfig
    },
    .afcfg = { GPIOD, GPIO_Pin_12, GPIO_PinSource12, GPIO_AF2_TIM4 },
  },
#elif BOARD_ROBERT_BB2 || BOARD_ROBERT_EVT
  .ctl = { GPIOA, GPIO_Pin_12, true },
  .pwm = {
    .output = { GPIOB, GPIO_Pin_14, true },
    .timer = {
      .peripheral = TIM12,
      .config_clock = RCC_APB1Periph_TIM12,
      .init = TIM_OC1Init,
      .preload = TIM_OC1PreloadConfig
    },
    .afcfg = { GPIOB, GPIO_Pin_14, GPIO_PinSource14, GPIO_AF9_TIM12 },
  },
#else
#error "Unknown board"
#endif
};

static const BoardConfigActuator BOARD_CONFIG_BACKLIGHT = {
  .options = ActuatorOptions_Pwm,
  .ctl = {0},
  .pwm = {
    .output = { GPIOG, GPIO_Pin_13, true },
    .timer = {
      .lp_peripheral = LPTIM1,
      .config_clock = RCC_APB1Periph_LPTIM1,
    },
    .afcfg = { GPIOG, GPIO_Pin_13, GPIO_PinSource13, GPIO_AF3_LPTIM1 },
  },
};

static const BoardConfigAccessory BOARD_CONFIG_ACCESSORY = {
#if BOARD_ROBERT_BB || BOARD_CUTTS_BB
  .power_en = { GPIOA, GPIO_Pin_11, true },
#elif BOARD_ROBERT_BB2 || BOARD_ROBERT_EVT
  .power_en = { GPIOD, GPIO_Pin_2, true },
#else
#error "Unknown board"
#endif
  .int_gpio = { GPIOH, GPIO_Pin_13 },
  .exti = { EXTI_PortSourceGPIOH, 13 },
};

static const BoardConfigBTCommon BOARD_CONFIG_BT_COMMON = {
  .controller = DA14681,
  .reset = { GPIOG, GPIO_Pin_0, true },
  .wakeup = {
    .int_gpio = { GPIOG, GPIO_Pin_1 },
    .int_exti = { EXTI_PortSourceGPIOG, 1 },
  },
};

static const BoardConfigBTUART BOARD_CONFIG_BT_UART = {
  .rx_af_cfg = { GPIOE, GPIO_Pin_0, GPIO_PinSource0, GPIO_AF8_UART8 },
  .tx_af_cfg = { GPIOE, GPIO_Pin_1, GPIO_PinSource1, GPIO_AF8_UART8 },
  .rx_clk_control = RCC_APB1Periph_UART8,
  .tx_clk_control = RCC_APB1Periph_UART8,
  .rx_uart = UART8,
  .tx_uart = UART8,
};

static const BoardConfigBTSPI BOARD_CONFIG_BT_SPI = {
  .cs = { GPIOE, GPIO_Pin_11, false },
};

static const BoardConfigMCO1 BOARD_CONFIG_MCO1 = {
  .output_enabled = true,
  .af_cfg = {
    .gpio = GPIOA,
    .gpio_pin = GPIO_Pin_8,
    .gpio_pin_source = GPIO_PinSource8,
    .gpio_af = GPIO_AF0_MCO,
  },
  .an_cfg = {
    .gpio = GPIOA,
    .gpio_pin = GPIO_Pin_8,
  },
};

#define DIALOG_TIMER_IRQ_HANDLER TIM6_DAC_IRQHandler
static const TimerIrqConfig BOARD_BT_WATCHDOG_TIMER = {
  .timer = {
    .peripheral = TIM6,
    .config_clock = RCC_APB1Periph_TIM6,
  },
  .irq_channel = TIM6_DAC_IRQn,
};

extern DMARequest * const COMPOSITOR_DMA;

extern UARTDevice * const QEMU_UART;
extern UARTDevice * const DBG_UART;
extern UARTDevice * const ACCESSORY_UART;
extern UARTDevice * const BT_TX_BOOTROM_UART;
extern UARTDevice * const BT_RX_BOOTROM_UART;

extern SPISlavePort * const BMI160_SPI;

extern I2CSlavePort * const I2C_MAX14690;
extern I2CSlavePort * const I2C_MAG3110;
extern I2CSlavePort * const I2C_AS7000;

extern VoltageMonitorDevice * const VOLTAGE_MONITOR_ALS;
extern VoltageMonitorDevice * const VOLTAGE_MONITOR_BATTERY;

extern TemperatureSensor * const TEMPERATURE_SENSOR;

extern QSPIPort * const QSPI;
extern QSPIFlash * const QSPI_FLASH;

extern ICE40LPDevice * const ICE40LP;
extern SPISlavePort * const DIALOG_SPI;

extern MicDevice * const MIC;

extern HRMDevice * const HRM;

#if BOARD_CUTTS_BB
extern TouchSensor * const EWD1000;
#endif
