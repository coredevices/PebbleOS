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

#include "drivers/battery.h"

#include "drivers/exti.h"
#include "drivers/gpio.h"
#include "board/board.h"
#include "drivers/otp.h"
#include "drivers/periph_config.h"
#include "system/logging.h"

#define STM32F2_COMPATIBLE
#include <mcu.h>

#include <stdlib.h>
#include <string.h>

#include "kernel/events.h"
#include "services/common/system_task.h"
#include "services/common/new_timer/new_timer.h"
#include "os/tick.h"

#include "FreeRTOS.h"
#include "timers.h"


static const uint32_t USB_CONN_DEBOUNCE_MS = 400;
static TimerID s_debounce_timer_handle = TIMER_INVALID_ID;
static bool s_debounced_is_usb_connected = false;

static bool battery_is_usb_connected_raw(void);

static void battery_vusb_interrupt_handler(bool *should_context_switch);

static void battery_conn_debounce_callback(void* data) {
  s_debounced_is_usb_connected = battery_is_usb_connected_raw();

  if (!s_debounced_is_usb_connected) {
    // disconnection event
    // - put the watch charger into a sane state
    // - disable fast-charge and re-enable the charger
    battery_set_charge_enable(true);
    battery_set_fast_charge(false);
  }

  PebbleEvent event = {
    .type = PEBBLE_BATTERY_CONNECTION_EVENT,
    .battery_connection = {
      .is_connected = s_debounced_is_usb_connected,
    }
  };

  event_put(&event);
}

static bool board_has_chg_fast() {
  return BOARD_CONFIG_POWER.chg_fast.gpio != 0;
}

static bool board_has_chg_en() {
  return BOARD_CONFIG_POWER.chg_en.gpio != 0;
}

// These are the guts of battery_set_charge_enable(), called when we already have periph_config_acquire_lock
static void prv_battery_set_charge_enable(bool charging_enabled) {
  if (board_has_chg_en()) {
    gpio_use(BOARD_CONFIG_POWER.chg_en.gpio);

    GPIO_WriteBit(BOARD_CONFIG_POWER.chg_en.gpio, BOARD_CONFIG_POWER.chg_en.gpio_pin, charging_enabled?Bit_SET:Bit_RESET);

    gpio_release(BOARD_CONFIG_POWER.chg_en.gpio);
    PBL_LOG(LOG_LEVEL_DEBUG, "Charging:%s", charging_enabled?"enabled":"disabled");
  }
}

// These are the guts of battery_set_fast_charge(), called when we already have periph_config_acquire_lock
static void prv_battery_set_fast_charge(bool fast_charge_enabled) {
  if (board_has_chg_fast()) {
    gpio_use(BOARD_CONFIG_POWER.chg_fast.gpio);

    GPIO_WriteBit(BOARD_CONFIG_POWER.chg_fast.gpio, BOARD_CONFIG_POWER.chg_fast.gpio_pin, fast_charge_enabled?Bit_RESET:Bit_SET);

    gpio_release(BOARD_CONFIG_POWER.chg_fast.gpio);
    PBL_LOG(LOG_LEVEL_DEBUG, "Fastcharge %s", fast_charge_enabled?"enabled":"disabled");
  }
}

void battery_init(void) {
  s_debounce_timer_handle = new_timer_create();

  periph_config_acquire_lock();
  gpio_use(BOARD_CONFIG_POWER.vusb_stat.gpio);
  gpio_use(BOARD_CONFIG_POWER.chg_stat.gpio);

  GPIO_InitTypeDef GPIO_InitStructure;
  GPIO_StructInit(&GPIO_InitStructure);

  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
  GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
  GPIO_InitStructure.GPIO_Pin = BOARD_CONFIG_POWER.vusb_stat.gpio_pin;
  GPIO_Init(BOARD_CONFIG_POWER.vusb_stat.gpio, &GPIO_InitStructure);

  GPIO_InitStructure.GPIO_Pin = BOARD_CONFIG_POWER.chg_stat.gpio_pin;
  GPIO_Init(BOARD_CONFIG_POWER.chg_stat.gpio, &GPIO_InitStructure);

  if (board_has_chg_fast() || board_has_chg_en()) {
    // Initialize PD2 as the sensor enable
    GPIO_StructInit(&GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_OD;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;

    if (board_has_chg_fast()) {
      gpio_use(BOARD_CONFIG_POWER.chg_fast.gpio);
      GPIO_InitStructure.GPIO_Pin = BOARD_CONFIG_POWER.chg_fast.gpio_pin;
      GPIO_Init(BOARD_CONFIG_POWER.chg_fast.gpio, &GPIO_InitStructure);
      prv_battery_set_fast_charge(false);
      gpio_release(BOARD_CONFIG_POWER.chg_fast.gpio);
    }

    if (board_has_chg_en()) {
      gpio_use(BOARD_CONFIG_POWER.chg_en.gpio);
      GPIO_InitStructure.GPIO_Pin = BOARD_CONFIG_POWER.chg_en.gpio_pin;
      GPIO_Init(BOARD_CONFIG_POWER.chg_en.gpio, &GPIO_InitStructure);
      prv_battery_set_charge_enable(true);
      gpio_release(BOARD_CONFIG_POWER.chg_en.gpio);
    }
  }

  if (BOARD_CONFIG_POWER.has_vusb_interrupt) {
    periph_config_release_lock();

    exti_configure_pin(BOARD_CONFIG_POWER.vusb_exti, ExtiTrigger_RisingFalling,
        battery_vusb_interrupt_handler);
    exti_enable(BOARD_CONFIG_POWER.vusb_exti);

    periph_config_acquire_lock();
  } else {
    // TODO: Start polling vusb_stat
  }

  gpio_release(BOARD_CONFIG_POWER.vusb_stat.gpio);
  gpio_release(BOARD_CONFIG_POWER.chg_stat.gpio);

  periph_config_release_lock();

  if (BOARD_CONFIG_POWER.has_vusb_interrupt) {
    // Prime the debounced state.
    s_debounced_is_usb_connected = battery_is_usb_connected_raw();
  }
}

bool battery_charge_controller_thinks_we_are_charging_impl(void) {
  periph_config_acquire_lock();
  gpio_use(BOARD_CONFIG_POWER.chg_stat.gpio);
  bool state = !GPIO_ReadInputDataBit(BOARD_CONFIG_POWER.chg_stat.gpio, BOARD_CONFIG_POWER.chg_stat.gpio_pin);
  gpio_release(BOARD_CONFIG_POWER.chg_stat.gpio);
  periph_config_release_lock();
  return state;
}

static bool battery_is_usb_connected_raw(void) {
  periph_config_acquire_lock();
  gpio_use(BOARD_CONFIG_POWER.vusb_stat.gpio);
  bool state = !GPIO_ReadInputDataBit(BOARD_CONFIG_POWER.vusb_stat.gpio, BOARD_CONFIG_POWER.vusb_stat.gpio_pin);
  gpio_release(BOARD_CONFIG_POWER.vusb_stat.gpio);
  periph_config_release_lock();
  return state;
}

bool battery_is_usb_connected_impl(void) {
  if (BOARD_CONFIG_POWER.has_vusb_interrupt) {
    return s_debounced_is_usb_connected;
  } else {
    return battery_is_usb_connected_raw();
  }
}


// This callback gets installed by DBG_SERIAL_FREERTOS_IRQHandler() using system_task_add_callback_from_isr().
// It is used to start up our timer since doing so from an ISR is not allowed. 
static void prv_start_timer_sys_task_callback(void* data) {
  new_timer_start(s_debounce_timer_handle, USB_CONN_DEBOUNCE_MS, battery_conn_debounce_callback, NULL, 0 /*flags*/);
}

static void battery_vusb_interrupt_handler(bool *should_context_switch) {
  // Start the timer from a system task callback - not allowed to do so from an ISR
  system_task_add_callback_from_isr(prv_start_timer_sys_task_callback, NULL, should_context_switch);
}

int battery_get_millivolts(void) {
  ADCVoltageMonitorReading info = battery_read_voltage_monitor();

  // Apologies for the madness numbers.
  // The previous implementation had some approximations in it. The battery voltage is scaled
  // down by a pair of resistors (750k at the top, 470k at the bottom), resulting in a required
  // scaling of (75 + 47) / 47 or roughly 2.56x, but the previous implementation also required
  // fudging the numbers a bit in order to approximate for leakage current (a 73/64 multiple
  // was arbitrarily increased to 295/256). In order to match this previous arbitrary scaling
  // I've chosen new numbers that provide 2.62x scaling, which is the previous 2.56x with the
  // same amount of fudging applied.

  return battery_convert_reading_to_millivolts(info, 3599, 1373);
}

extern void command_sim_battery_connection(const char *bool_str) {
  bool value = atoi(bool_str);

  PebbleEvent event = {
    .type = PEBBLE_BATTERY_CONNECTION_EVENT,
    .battery_connection = {
      .is_connected = value,
    }
  };
  event_put(&event);
}

void battery_set_charge_enable(bool charging_enabled) {
  periph_config_acquire_lock();
  prv_battery_set_charge_enable(charging_enabled);
  periph_config_release_lock();
}

void battery_set_fast_charge(bool fast_charge_enabled) {
  periph_config_acquire_lock();
  prv_battery_set_fast_charge(fast_charge_enabled);
  periph_config_release_lock();
}

