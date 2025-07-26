#include "drivers/debounced_button.h"

#include "board/board.h"
#include "drivers/button.h"
#include "drivers/exti.h"
#include "drivers/gpio.h"
#include "drivers/periph_config.h"
#include "kernel/events.h"
#include "kernel/util/stop.h"
#include "system/bootbits.h"
#include "util/bitset.h"
#include "kernel/util/sleep.h"

#define SF32LB52_COMPATIBLE
#include "mcu.h"

#include "projdefs.h"

/* Timer period 1us, auto reload is 2ms. */
#define BUTTON_TIMER_FREQUENCY_HZ     1000000
#define BUTTON_TIMER_AUTO_RELOAD_TIME 2000

// A button must be stable for 20 samples (40ms) to be accepted.
static const uint32_t NUM_DEBOUNCE_SAMPLES = 20;
static GPT_HandleTypeDef TIM_Handle = {0};

static void prv_timer_handler(void);

static void initialize_button_timer(void) {
  TIM_Handle.Instance = GPTIM1;
  TIM_Handle.Init.Prescaler = HAL_RCC_GetPCLKFreq(CORE_ID_HCPU, 1) / BUTTON_TIMER_FREQUENCY_HZ - 1;
  TIM_Handle.core = CORE_ID_HCPU;
  TIM_Handle.Init.CounterMode = GPT_COUNTERMODE_UP;
  TIM_Handle.Init.RepetitionCounter = 0;
  HAL_GPT_Base_Init(&TIM_Handle);

  /* Default NVIC priority group set in exit driver, group_2 */
  HAL_NVIC_SetPriority(GPTIM1_IRQn, 3, 0);
  HAL_NVIC_EnableIRQ(GPTIM1_IRQn);
  
  __HAL_GPT_SET_AUTORELOAD(&TIM_Handle, BUTTON_TIMER_AUTO_RELOAD_TIME);
  __HAL_GPT_SET_MODE(&TIM_Handle, GPT_OPMODE_REPETITIVE);
}

static bool prv_check_timer_enabled(void) {
  return (HAL_GPT_Base_GetState(&TIM_Handle) != HAL_GPT_STATE_READY);
}

static void disable_button_timer(void) {
  if (prv_check_timer_enabled()) {
    HAL_GPT_Base_Stop_IT(&TIM_Handle);
    stop_mode_enable(InhibitorButton);
  }
}

static void prv_enable_button_timer(void) {
  __disable_irq();
  if (!prv_check_timer_enabled()) {
    HAL_GPT_Base_Start_IT(&TIM_Handle);
    stop_mode_disable(InhibitorButton);
  }
  __enable_irq();
}

static void prv_button_interrupt_handler(bool *should_context_switch) {
  prv_enable_button_timer();
}

void debounced_button_init(void) {
  button_init();

  for (int i = 0; i < OBELIX_BUTTON_NUM; ++i) {
    const ExtiConfig config = BOARD_CONFIG_BUTTON.buttons[i].gpioe;
    exti_configure_pin(config, ExtiTrigger_Rising, prv_button_interrupt_handler);
    exti_enable(config);
  }

  initialize_button_timer();

  if (button_get_state_bits() != 0) {
     prv_enable_button_timer();
  }
}

void GPTIM1_IRQHandler(void)
{
  HAL_GPT_IRQHandler(&TIM_Handle);
}

void HAL_GPT_PeriodElapsedCallback(GPT_HandleTypeDef *htim)
{
  prv_timer_handler();
}

static void prv_timer_handler(void) {
  bool should_context_switch = pdFALSE;
  bool can_power_down_tim4 = true;

  static uint32_t s_button_timers[] = {0, 0};
  static uint32_t s_debounced_button_state = 0;

  for (int i = 0; i < OBELIX_BUTTON_NUM; ++i) {
    bool debounced_button_state = bitset32_get(&s_debounced_button_state, i);
    bool is_pressed = button_is_pressed(i);

    if (is_pressed == debounced_button_state) {
      s_button_timers[i] = 0;
      continue;
    }

    can_power_down_tim4 = false;

    s_button_timers[i] += 1;

    if (s_button_timers[i] == NUM_DEBOUNCE_SAMPLES) {
      s_button_timers[i] = 0;

      /* Debug purpose, will be removed before PR merge. */
      PBL_LOG(LOG_LEVEL_DEBUG, "Button %d %s", i,
             (is_pressed) ? "pressed" : "released");
    
      bitset32_update(&s_debounced_button_state, i, is_pressed);

      PebbleEvent e = {
        .type = (is_pressed) ? PEBBLE_BUTTON_DOWN_EVENT : PEBBLE_BUTTON_UP_EVENT,
        .button.button_id = i
      };
      should_context_switch = event_put_isr(&e);
    }
  }

  if (can_power_down_tim4) {
    __disable_irq();
    disable_button_timer();
    __enable_irq();
  }

  portEND_SWITCHING_ISR(should_context_switch);
}

// Serial commands
///////////////////////////////////////////////////////////
void command_put_raw_button_event(const char* button_index, const char* is_button_down_event) {

}
