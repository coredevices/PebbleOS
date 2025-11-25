/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "drivers/exti.h"

#include "board/board.h"
#include "drivers/periph_config.h"
#include "kernel/events.h"
#include "mcu/interrupts.h"
#include "system/passert.h"

#define STM32F2_COMPATIBLE
#define STM32F4_COMPATIBLE
#define STM32F7_COMPATIBLE
#include <mcu.h>

#include <stdbool.h>

//! Tracks whether we've disabled interrupts as part of locking out other people from our EXTI
//! registers.
static bool s_exti_locked = false;

//! Have we already configured the EXTI9_5_IRQn interrupt?
static bool s_9_5_nvic_configured = false;
//! Have we already configured the EXTI15_10_IRQn interrupt?
static bool s_15_10_nvic_configured = false;

static ExtiHandlerCallback s_exti_handlers[16];

//! Convert a exti number (value 0 to 22) to one of the EXTI_LineX defines
static uint32_t prv_exti_line_to_bit(int exti_line) {
  return 0x1 << exti_line;
}

static void prv_lock(void) {
  if (mcu_state_are_interrupts_enabled()) {
    __disable_irq();
    s_exti_locked = true;
  }
}

static void prv_unlock(void) {
  if (s_exti_locked) {
    __enable_irq();
    s_exti_locked = false;
  }
}

static IRQn_Type prv_get_irq_enum(int exti_line) {
  if (exti_line <= 4) {
    return EXTI0_IRQn + exti_line;
  }
  if (exti_line <= 9) {
    return EXTI9_5_IRQn;
  }
  if (exti_line <= 15) {
    return EXTI15_10_IRQn;
  }
  if (exti_line == ExtiLineOther_RTCAlarm) {
    return RTC_Alarm_IRQn;
  }
  if (exti_line == ExtiLineOther_RTCWakeup) {
    return RTC_WKUP_IRQn;
  }
  WTF;
}

static void prv_configure_nvic_channel(IRQn_Type irqn) {
  NVIC_SetPriority(irqn, EXTI_PRIORITY);
  NVIC_EnableIRQ(irqn);
}

static void prv_check_nvic_channel(IRQn_Type irqn) {
  // Make sure we haven't already set up the channel in question.
  if (irqn == EXTI9_5_IRQn) {
    if (s_9_5_nvic_configured) {
      return; // Already configured
    }
    s_9_5_nvic_configured = true;
  } else if (irqn == EXTI15_10_IRQn) {
    if (s_15_10_nvic_configured) {
      return; // Already configured
    }
    s_15_10_nvic_configured = true;
  }

  prv_configure_nvic_channel(irqn);
}

void exti_configure_pin(ExtiConfig cfg, ExtiTrigger trigger, ExtiHandlerCallback cb) {
  periph_config_acquire_lock();
  periph_config_enable(SYSCFG, RCC_APB2Periph_SYSCFG);

  // Select which GPIO to monitor
  SYSCFG->EXTICR[cfg.exti_line >> 0x02] &=
        ~(((uint32_t)0x0F) << (0x04 * (cfg.exti_line & (uint8_t)0x03)));
  SYSCFG->EXTICR[cfg.exti_line >> 0x02] |=
        (((uint32_t)cfg.exti_port_source) << (0x04 * (cfg.exti_line & (uint8_t)0x03)));

  periph_config_disable(SYSCFG, RCC_APB2Periph_SYSCFG);
  periph_config_release_lock();

  s_exti_handlers[cfg.exti_line] = cb;

  // Do the rest of the configuration
  exti_configure_other(cfg.exti_line, trigger);
}

void exti_configure_other(ExtiLineOther exti_line, ExtiTrigger trigger) {
  // Clear IT Pending bit
  EXTI->PR = prv_exti_line_to_bit(exti_line);

  prv_lock();

  switch (trigger) {
  case ExtiTrigger_Rising:
    EXTI->RTSR |= prv_exti_line_to_bit(exti_line);
    EXTI->FTSR &= ~prv_exti_line_to_bit(exti_line);
    break;
  case ExtiTrigger_Falling:
    EXTI->RTSR &= ~prv_exti_line_to_bit(exti_line);
    EXTI->FTSR |= prv_exti_line_to_bit(exti_line);
    break;
  case ExtiTrigger_RisingFalling:
    EXTI->RTSR |= prv_exti_line_to_bit(exti_line);
    EXTI->FTSR |= prv_exti_line_to_bit(exti_line);
    break;
  }

  prv_unlock();

  periph_config_acquire_lock();
  prv_check_nvic_channel(prv_get_irq_enum(exti_line));
  periph_config_release_lock();
}

void exti_enable_other(ExtiLineOther exti_line) {
  prv_lock();

  EXTI->IMR |= prv_exti_line_to_bit(exti_line);

  prv_unlock();
}

void exti_disable_other(ExtiLineOther exti_line) {
  prv_lock();

  uint32_t exti_bit = prv_exti_line_to_bit(exti_line);

  EXTI->IMR &= ~exti_bit;
  EXTI->PR = exti_bit;

  // No need to disable the NVIC ISR. If all the EXTIs that feed a given shared ISR are disabled
  // the ISR won't fire.

  prv_unlock();
}

void exti_set_pending(ExtiConfig cfg) {
  IRQn_Type irq;
  switch (cfg.exti_line) {
    case 0: irq = EXTI0_IRQn; break;
    case 1: irq = EXTI1_IRQn; break;
    case 2: irq = EXTI2_IRQn; break;
    case 3: irq = EXTI3_IRQn; break;
    case 4: irq = EXTI4_IRQn; break;
    case 5 ... 9: irq = EXTI9_5_IRQn; break;
    case 10 ... 15: irq = EXTI15_10_IRQn; break;
    default:
      WTF;
  }
  NVIC_SetPendingIRQ(irq);
}

void exti_clear_pending_other(ExtiLineOther exti_line) {
  uint32_t exti_bit = prv_exti_line_to_bit(exti_line);
  EXTI->PR = exti_bit;
}

// Helper functions for handling ISRs
///////////////////////////////////////////////////////////////////////////////

static void prv_handle_exti(int exti_line) {
  // Clear IT Pending bit
  EXTI->PR = prv_exti_line_to_bit(exti_line);

  const ExtiHandlerCallback cb = s_exti_handlers[exti_line];
  if (cb) {
    bool should_context_switch = false;
    cb(&should_context_switch);
    portEND_SWITCHING_ISR(should_context_switch);
  }
}

static void prv_check_handle_exti(int exti_line) {
  if (EXTI->PR & prv_exti_line_to_bit(exti_line)) {
    prv_handle_exti(exti_line);
  }
}


// Actual ISR functions
///////////////////////////////////////////////////////////////////////////////

void EXTI0_IRQHandler(void) {
  NVIC_ClearPendingIRQ(EXTI0_IRQn);
  prv_handle_exti(0);
}

void EXTI1_IRQHandler(void) {
  NVIC_ClearPendingIRQ(EXTI1_IRQn);
  prv_handle_exti(1);
}

void EXTI2_IRQHandler(void) {
  NVIC_ClearPendingIRQ(EXTI2_IRQn);
  prv_handle_exti(2);
}

void EXTI3_IRQHandler(void) {
  NVIC_ClearPendingIRQ(EXTI3_IRQn);
  prv_handle_exti(3);
}

void EXTI4_IRQHandler(void) {
  NVIC_ClearPendingIRQ(EXTI4_IRQn);
  prv_handle_exti(4);
}

void EXTI9_5_IRQHandler(void) {
  NVIC_ClearPendingIRQ(EXTI9_5_IRQn);
  for (int i = 5; i <= 9; ++i) {
    prv_check_handle_exti(i);
  }
}

void EXTI15_10_IRQHandler(void) {
  NVIC_ClearPendingIRQ(EXTI15_10_IRQn);
  for (int i = 10; i <= 15; ++i) {
    prv_check_handle_exti(i);
  }
}

