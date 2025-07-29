/*
 * Copyright 2025 SiFli Technologies(Nanjing) Co., Ltd
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

#include "drivers/debounced_button.h"

#include "board/board.h"
#include "drivers/button.h"
#include "drivers/exti.h"
#include "drivers/gpio.h"
#include "drivers/periph_config.h"
#include "kernel/events.h"
#include "kernel/util/stop.h"
#include "system/bootbits.h"
#include "system/reset.h"
#include "util/bitset.h"
#include "kernel/util/sleep.h"
#include "bf0_hal_tim.h"

#include "FreeRTOS.h"

/* Timer period 1us, auto reload is 2ms. */
#define TIMER_FREQUENCY_HZ    1000000
#define TIMER_PERIOD_TICKS    2000

#define RESET_BUTTONS ((1 << BUTTON_ID_SELECT) | (1 << BUTTON_ID_BACK))

#define DEBOUNCE_SAMPLES_PER_SECOND (TIMER_FREQUENCY_HZ / TIMER_PERIOD_TICKS)

// This reset-buttons-held timeout must be lower than the PMIC's back-button-reset timeout,
// which is ~8-11s. The spacing between these timeouts should be large enough to avoid accidentally
// shutting down the device when a customer is attempting to reset. Therefore the FW's
// reset-buttons-held timeout is set to 5 seconds:
#define RESET_THRESHOLD_SAMPLES (5 * DEBOUNCE_SAMPLES_PER_SECOND)

// A button must be stable for 20 samples (40ms) to be accepted.
static const uint32_t s_num_debounce_samples = 20;
static GPT_HandleTypeDef s_tim_hdl = {0};

static void prv_timer_handler(void);

static void initialize_button_timer(void) {
  s_tim_hdl.Instance = BOARD_CONFIG_BUTTON.timer;
  s_tim_hdl.Init.Prescaler = HAL_RCC_GetPCLKFreq(CORE_ID_HCPU, 1) / TIMER_FREQUENCY_HZ - 1;
  s_tim_hdl.core = CORE_ID_HCPU;
  s_tim_hdl.Init.CounterMode = GPT_COUNTERMODE_UP;
  s_tim_hdl.Init.RepetitionCounter = 0;
  HAL_GPT_Base_Init(&s_tim_hdl);

  /* Default NVIC priority group set in exit driver, group_2 */
  HAL_NVIC_SetPriority(BOARD_CONFIG_BUTTON.timer_irqn, 3, 1);
  HAL_NVIC_EnableIRQ(BOARD_CONFIG_BUTTON.timer_irqn);

  __HAL_GPT_SET_AUTORELOAD(&s_tim_hdl, TIMER_PERIOD_TICKS);
  __HAL_GPT_SET_MODE(&s_tim_hdl, GPT_OPMODE_REPETITIVE);
}

static bool prv_check_timer_enabled(void) {
  return (HAL_GPT_Base_GetState(&s_tim_hdl) != HAL_GPT_STATE_READY);
}

static void disable_button_timer(void) {
  if (prv_check_timer_enabled()) {
    HAL_GPT_Base_Stop_IT(&s_tim_hdl);
    stop_mode_enable(InhibitorButton);
  }
}

static void prv_enable_button_timer(void) {
  __disable_irq();
  if (!prv_check_timer_enabled()) {
    HAL_GPT_Base_Start_IT(&s_tim_hdl);
    stop_mode_disable(InhibitorButton);
  }
  __enable_irq();
}

static void prv_button_interrupt_handler(bool *should_context_switch) {
  prv_enable_button_timer();
}

void debounced_button_init(void) {
  button_init();

  for (int i = 0; i < NUM_BUTTONS; ++i) {
    const ExtiConfig config = {
      .peripheral = BOARD_CONFIG_BUTTON.buttons[i].port,
      .gpio_pin = BOARD_CONFIG_BUTTON.buttons[i].pin
    };
    exti_configure_pin(config, ExtiTrigger_RisingFalling, prv_button_interrupt_handler);
    exti_enable(config);
  }

  initialize_button_timer();

  if (button_get_state_bits() != 0) {
    prv_enable_button_timer();
  }
}

void debounced_button_irq_handler(GPT_TypeDef *timer)
{
  if (__HAL_GPT_GET_FLAG(&s_tim_hdl, GPT_FLAG_UPDATE) != RESET) {
    if (__HAL_GPT_GET_IT_SOURCE(&s_tim_hdl, GPT_IT_UPDATE) != RESET) {
      __HAL_GPT_CLEAR_IT(&s_tim_hdl, GPT_IT_UPDATE);
      prv_timer_handler();
    }
  }
}

static void prv_timer_handler(void) {
  bool should_context_switch = pdFALSE;
  bool can_disable_timer = true;

  static uint32_t s_button_timers[NUM_BUTTONS] = {0, 0, 0, 0};
  static uint32_t s_debounced_button_state = 0;

  for (int i = 0; i < NUM_BUTTONS; ++i) {
    bool debounced_button_state = bitset32_get(&s_debounced_button_state, i);
    bool is_pressed = button_is_pressed(i);

    if (is_pressed == debounced_button_state) {
      s_button_timers[i] = 0;
      continue;
    }

    can_disable_timer = false;

    s_button_timers[i] += 1;

    if (s_button_timers[i] == s_num_debounce_samples) {
      s_button_timers[i] = 0;

      bitset32_update(&s_debounced_button_state, i, is_pressed);

      PebbleEvent e = {
        .type = (is_pressed) ? PEBBLE_BUTTON_DOWN_EVENT : PEBBLE_BUTTON_UP_EVENT,
        .button.button_id = i
      };
      should_context_switch = event_put_isr(&e);
    }
  }

#if !defined(MANUFACTURING_FW)
  // Now that s_debounced_button_state is updated, check to see if the user is holding down the reset
  // combination.
  static uint32_t s_hard_reset_timer = 0;
  if ((s_debounced_button_state & RESET_BUTTONS) == RESET_BUTTONS) {
    s_hard_reset_timer += 1;
    can_disable_timer = false;

    if (s_hard_reset_timer > RESET_THRESHOLD_SAMPLES) {
      __disable_irq();

      // If the UP button is held at the moment the timeout is hit, set the force-PRF bootbit:
      const bool force_prf = (s_debounced_button_state & (1 << BUTTON_ID_UP));
      if (force_prf) {
        boot_bit_set(BOOT_BIT_FORCE_PRF);
      }

      RebootReason reason = {
        .code = force_prf ? RebootReasonCode_PrfResetButtonsHeld :
                            RebootReasonCode_ResetButtonsHeld
      };
      reboot_reason_set(&reason);

      // Don't use system_reset here. This back door absolutely must work. Just hard reset.
      system_hard_reset();
    }
  } else {
    s_hard_reset_timer = 0;
  }
#endif

  if (can_disable_timer) {
    __disable_irq();
    disable_button_timer();
    __enable_irq();
  }

  portEND_SWITCHING_ISR(should_context_switch);
}

// Serial commands
///////////////////////////////////////////////////////////
void command_put_raw_button_event(const char* button_index, const char* is_button_down_event) {
  PebbleEvent e;
  int is_down = atoi(is_button_down_event);
  int button = atoi(button_index);

  if ((button < 0 || button > NUM_BUTTONS) || (is_down != 1 && is_down != 0)) {
    return;
  }
  e.type = (is_down) ? PEBBLE_BUTTON_DOWN_EVENT : PEBBLE_BUTTON_UP_EVENT;
  e.button.button_id = button;
  event_put(&e);
}
