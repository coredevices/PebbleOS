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

#include "drivers/button.h"
#include "services/imu/units.h"
#include "util/size.h"

#define BT_VENDOR_ID 0x0154
#define BT_VENDOR_NAME "Pebble Technology"

#define BOARD_LSE_MODE RCC_LSE_ON

static const BoardConfig BOARD_CONFIG = {
  .ambient_light_dark_threshold = 3000,
  .ambient_k_delta_threshold = 96,
  .photo_en = { GPIOD, GPIO_Pin_2, true },

  .dbgserial_int = { EXTI_PortSourceGPIOC, 11 },

  .lcd_com = { GPIOB, GPIO_Pin_1, true },

  .power_ctl_5v0 = { GPIOC, GPIO_Pin_5, false },

  .backlight_on_percent = 25,
  .backlight_max_duty_cycle_percent = 100,

  .power_5v0_options = OptionActiveLowOpenDrain,

  .has_mic = false,
};

static const BoardConfigButton BOARD_CONFIG_BUTTON = {
  .buttons = {
    [BUTTON_ID_BACK]    = { "Back",   GPIOC, GPIO_Pin_3, { EXTI_PortSourceGPIOC, 3 }, GPIO_PuPd_NOPULL },
    [BUTTON_ID_UP]      = { "Up",     GPIOA, GPIO_Pin_2, { EXTI_PortSourceGPIOA, 2 }, GPIO_PuPd_NOPULL },
    [BUTTON_ID_SELECT]  = { "Select", GPIOC, GPIO_Pin_6, { EXTI_PortSourceGPIOC, 6 }, GPIO_PuPd_NOPULL },
    [BUTTON_ID_DOWN]    = { "Down",   GPIOA, GPIO_Pin_1, { EXTI_PortSourceGPIOA, 1 }, GPIO_PuPd_NOPULL },
  },

  .button_com = { GPIOA, GPIO_Pin_0 },
  .active_high = false,
};

static const BoardConfigPower BOARD_CONFIG_POWER = {
  .vusb_stat = {
    .gpio = GPIOC,
    .gpio_pin = GPIO_Pin_13,
  },
  .vusb_exti = { EXTI_PortSourceGPIOC, 13 },

  .chg_stat = { GPIOH, GPIO_Pin_1 },
  .chg_fast = { GPIOB, GPIO_Pin_6 },
  .chg_en = { GPIOB, GPIO_Pin_9 },

  .has_vusb_interrupt = true,

  .wake_on_usb_power = true,

  .charging_status_led_voltage_compensation = 0,

  .low_power_threshold = 5,
  .battery_capacity_hours = 144,
};

static const BoardConfigAccel BOARD_CONFIG_ACCEL = {
  .accel_config = {
      .axes_offsets[AXIS_X] = 1,
      .axes_offsets[AXIS_Y] = 0,
      .axes_offsets[AXIS_Z] = 2,
      .axes_inverts[AXIS_X] = true,
      .axes_inverts[AXIS_Y] = false,
      .axes_inverts[AXIS_Z] = true,
      .shake_thresholds[AccelThresholdHigh] = 0x7f,
      .shake_thresholds[AccelThresholdLow] = 0xa,
  },
  .accel_ints = {
    [0] = { EXTI_PortSourceGPIOC, 8 },
    [1] = { EXTI_PortSourceGPIOC, 9 }
  },
};

static const BoardConfigMag BOARD_CONFIG_MAG = {
  .mag_config = { // align raw mag data with accel coords (ENU)
    .axes_offsets[AXIS_X] = 1,
    .axes_offsets[AXIS_Y] = 0,
    .axes_offsets[AXIS_Z] = 2,
    .axes_inverts[AXIS_X] = false,
    .axes_inverts[AXIS_Y] = true,
    .axes_inverts[AXIS_Z] = true,
  },
  .mag_int = { EXTI_PortSourceGPIOC, 4 },
};

static const BoardConfigActuator BOARD_CONFIG_VIBE = {
  .options = ActuatorOptions_Ctl,
  .ctl = { GPIOB, GPIO_Pin_13, true },
};

static const BoardConfigActuator BOARD_CONFIG_BACKLIGHT = {
  .options = ActuatorOptions_Pwm,
  .ctl = {0},
  .pwm = {
    .output = { GPIOB, GPIO_Pin_5, true },
    .timer = {
      .peripheral = TIM3,
      .config_clock = RCC_APB1Periph_TIM3,
      .init = TIM_OC2Init,
      .preload = TIM_OC2PreloadConfig
    },
    .afcfg = { GPIOB, GPIO_Pin_5, GPIO_PinSource5, GPIO_AF_TIM3 },
  },
};

#define BOARD_BT_USART_IRQ_HANDLER USART1_IRQHandler
static const BoardConfigBTCommon BOARD_CONFIG_BT_COMMON = {
  .controller = CC2564A,
  .shutdown = { GPIOA, GPIO_Pin_3, false},
  .wakeup = {
    .int_gpio = { GPIOC, GPIO_Pin_12 },
    .int_exti = { EXTI_PortSourceGPIOC, 12 },
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

static const BoardConfigSharpDisplay BOARD_CONFIG_DISPLAY = {
  .spi = SPI2,
  .spi_gpio = GPIOB,
  .spi_clk = RCC_APB1Periph_SPI2,
  .spi_clk_periph = SpiPeriphClockAPB1,

  .clk = { GPIOB, GPIO_Pin_13, GPIO_PinSource13, GPIO_AF_SPI2 },
  .mosi = { GPIOB, GPIO_Pin_15, GPIO_PinSource15, GPIO_AF_SPI2 },
  .cs = { GPIOB, GPIO_Pin_12, true },

  .on_ctrl = { GPIOB, GPIO_Pin_14, true },
  .on_ctrl_otype = GPIO_OType_OD,
};

extern DMARequest * const SHARP_SPI_TX_DMA;

extern UARTDevice * const QEMU_UART;
extern UARTDevice * const DBG_UART;

extern I2CSlavePort * const I2C_LIS3DH;
extern I2CSlavePort * const I2C_MFI;
extern I2CSlavePort * const I2C_MAG3110;
extern I2CSlavePort * const I2C_LED;

extern VoltageMonitorDevice * const VOLTAGE_MONITOR_ALS;
extern VoltageMonitorDevice * const VOLTAGE_MONITOR_BATTERY;
