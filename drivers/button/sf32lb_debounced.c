/* SPDX-FileCopyrightText: 2025 SiFli Technologies(Nanjing) Co., Ltd */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/drivers/button.h>
#include <pbl/drivers/debounced_button.h>
#include <pbl/drivers/gpio.h>
#include <pbl/input/input.h>
#include <pbl/kernel/assert.h>
#include <pbl/kernel/irq.h>
#include <pbl/soc/sf32lb/sleep.h>
#include <pbl/util/bitops.h>

#include <bf0_hal_tim.h>
#include <board/board.h>

/* Timer period 100us, auto reload is 2ms. */
#define TIMER_FREQUENCY_HZ 10000
#define TIMER_PERIOD_TICKS 20

// A button must be stable for 20 samples (40ms) to be accepted.
static const uint32_t s_num_debounce_samples = 20;
static GPT_HandleTypeDef s_tim_hdl = {};
static bool s_timer_enabled = false;

static void prv_timer_handler(void);

static void initialize_button_timer(void) {
  s_tim_hdl.Instance = BOARD_CONFIG_BUTTON.timer;
  s_tim_hdl.Init.Prescaler = 24000000U / (TIMER_FREQUENCY_HZ - 1); // GPTIM2 clock is 24MHz
  s_tim_hdl.core = CORE_ID_HCPU;
  s_tim_hdl.Init.CounterMode = GPT_COUNTERMODE_UP;
  s_tim_hdl.Init.RepetitionCounter = 0;
  s_tim_hdl.Init.Period = TIMER_PERIOD_TICKS;
  HAL_GPT_Base_Init(&s_tim_hdl);

  pbl_irq_enable(BOARD_CONFIG_BUTTON.timer_irqn);

  __HAL_GPT_CLEAR_FLAG(&s_tim_hdl, GPT_FLAG_UPDATE);
  __HAL_GPT_URS_ENABLE(&s_tim_hdl);
  __HAL_GPT_SET_MODE(&s_tim_hdl, GPT_OPMODE_REPETITIVE);
}

static void disable_button_timer(void) {
  if (s_timer_enabled) {
    s_timer_enabled = false;
    HAL_GPT_Base_Stop_IT(&s_tim_hdl);
    soc_sf32lb_sleep_release(SOC_SF32LB_DEEPSLEEP);
  }
}

static void prv_enable_button_timer(void) {
  __disable_irq();
  if (!s_timer_enabled) {
    s_timer_enabled = true;
    HAL_GPT_Base_Start_IT(&s_tim_hdl);
    soc_sf32lb_sleep_block(SOC_SF32LB_DEEPSLEEP);
  }
  __enable_irq();
}

static void prv_button_interrupt_handler(void *arg) {
  prv_enable_button_timer();
}

void debounced_button_init(void) {
  button_init();

  for (int i = 0; i < NUM_BUTTONS; ++i) {
    struct pbl_irq_spec irq;

    PBL_ASSERTN(
        pbl_gpio_to_irq(&BOARD_CONFIG_BUTTON.buttons[i].gpio, PBL_IRQ_TYPE_EDGE_BOTH, &irq) == 0);
    PBL_ASSERTN(pbl_device_init(&irq.intc->dev) == 0);
    PBL_ASSERTN(pbl_intc_request(&irq, prv_button_interrupt_handler, nullptr) == 0);
    PBL_ASSERTN(pbl_intc_enable(&irq) == 0);
  }

  initialize_button_timer();

  if (button_get_state_bits() != 0) {
    prv_enable_button_timer();
  }
}

void debounced_button_irq_handler(GPT_TypeDef *timer) {
  if (__HAL_GPT_GET_FLAG(&s_tim_hdl, GPT_FLAG_UPDATE) != RESET) {
    if (__HAL_GPT_GET_IT_SOURCE(&s_tim_hdl, GPT_IT_UPDATE) != RESET) {
      __HAL_GPT_CLEAR_IT(&s_tim_hdl, GPT_IT_UPDATE);
      prv_timer_handler();
    }
  }
}

static void prv_timer_handler(void) {
  bool can_disable_timer = true;

  static uint32_t s_button_timers[NUM_BUTTONS] = {0, 0, 0, 0};
  static uint32_t s_debounced_button_state = 0;

  for (int i = 0; i < NUM_BUTTONS; ++i) {
    bool debounced_button_state = pbl_bitset32_get(&s_debounced_button_state, i);
    bool is_pressed = button_is_pressed(i);

    if (is_pressed == debounced_button_state) {
      s_button_timers[i] = 0;
      continue;
    }

    can_disable_timer = false;

    s_button_timers[i] += 1;

    if (s_button_timers[i] == s_num_debounce_samples) {
      s_button_timers[i] = 0;

      pbl_bitset32_update(&s_debounced_button_state, i, is_pressed);

      pbl_input_report_key(BOARD_CONFIG_BUTTON.buttons[i].code, is_pressed, true);
    }
  }

  if (can_disable_timer) {
    __disable_irq();
    disable_button_timer();
    __enable_irq();
  }
}
