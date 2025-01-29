#include "drivers/exti.h"

#include "board/board.h"
#include "drivers/periph_config.h"
#include "kernel/events.h"
#include "mcu/interrupts.h"
#include "system/passert.h"

#define NRF5_COMPATIBLE
#include <mcu.h>

#include <stdbool.h>

// NRF5 emulates EXTI using GPIOTE

static void *s_contexts[32 + P1_PIN_NUM] = {};

static void prv_exti_handler(nrfx_gpiote_pin_t pin, nrf_gpiote_polarity_t trigger) {
  ExtiHandlerCallback cb = (ExtiHandlerCallback) s_contexts[pin];
  
  bool should_context_switch = false;
  cb(&should_context_switch);
  
  portEND_SWITCHING_ISR(should_context_switch);
}

void exti_configure_pin(ExtiConfig cfg, ExtiTrigger trigger, ExtiHandlerCallback cb) {
  nrfx_err_t err;
  if (!nrfx_gpiote_is_init()) {
    err = nrfx_gpiote_init();
    PBL_ASSERTN(err == NRFX_SUCCESS);
  }
  s_contexts[cfg.gpio_pin] = cb;
  nrfx_gpiote_in_config_t pcfg = {
    .sense = trigger == ExtiTrigger_Rising ? NRF_GPIOTE_POLARITY_LOTOHI :
             trigger == ExtiTrigger_Falling ? NRF_GPIOTE_POLARITY_HITOLO :
             trigger == ExtiTrigger_RisingFalling ? NRF_GPIOTE_POLARITY_TOGGLE :
             -1,
    .pull = NRF_GPIO_PIN_NOPULL,
    .skip_gpio_setup = true,
  };
  nrfx_gpiote_in_init(cfg.gpio_pin, &pcfg, prv_exti_handler);
  nrfx_gpiote_in_event_disable(cfg.gpio_pin);
}

void exti_configure_other(ExtiLineOther exti_line, ExtiTrigger trigger) {
  WTF;
}

void exti_enable_other(ExtiLineOther exti_line) {
  WTF;
}

void exti_disable_other(ExtiLineOther exti_line) {
  WTF;
}

void exti_set_pending(ExtiConfig cfg) {
  WTF;
}

void exti_clear_pending_other(ExtiLineOther exti_line) {
  WTF;
}

void exti_enable(ExtiConfig cfg) {
  nrfx_gpiote_in_event_enable(cfg.gpio_pin, true /* int_enable */);
}

void exti_disable(ExtiConfig cfg) {
  nrfx_gpiote_in_event_disable(cfg.gpio_pin);
}

