/* SPDX-FileCopyrightText: 2025 SiFli Technologies(Nanjing) Co., Ltd */
/* SPDX-License-Identifier: Apache-2.0 */

#include "drivers/exti.h"

#include <stdbool.h>

#include "board/board.h"
#include "drivers/periph_config.h"
#include "kernel/events.h"
#include "mcu/interrupts.h"
#include "system/passert.h"

#define EXTI_MAX_GPIO1_PIN_NUM 16

typedef struct {
  uint32_t gpio_pin;
  ExtiHandlerCallback callback;
} ExtiHandlerConfig_t;

static ExtiHandlerConfig_t s_exti_gpio1_handler_configs[EXTI_MAX_GPIO1_PIN_NUM];

static GPIO_TypeDef *prv_gpio_get_instance(GPIO_TypeDef *hgpio, uint16_t gpio_pin,
                                           uint16_t *offset) {
  uint16_t inst_idx;
  GPIO_TypeDef *gpiox;

  HAL_ASSERT(gpio_pin < GPIO1_PIN_NUM);

  if (gpio_pin >= GPIO1_PIN_NUM) {
    return (GPIO_TypeDef *)NULL;
  }

  // There are many groups of similar registers in the GPIO, and because of register length limitations, up to 32 gpio can be operated in each group.
  inst_idx = gpio_pin >> 5;
  *offset = gpio_pin & 31;

  gpiox = (GPIO_TypeDef *)hgpio + inst_idx;

  return gpiox;
}

static void prv_insert_handler(GPIO_TypeDef *hgpio, uint8_t gpio_pin, ExtiHandlerCallback cb) {
  // Find the handler index for this pin
  uint8_t index = 0;
  while (index < EXTI_MAX_GPIO1_PIN_NUM &&
         s_exti_gpio1_handler_configs[index].callback != NULL) {
    index++;
  }
  if (index >= EXTI_MAX_GPIO1_PIN_NUM) {
    // No available slot
    return;
  }
  // Store the callback and index
  s_exti_gpio1_handler_configs[index].gpio_pin = gpio_pin;
  s_exti_gpio1_handler_configs[index].callback = cb;
}

static void prv_delete_handler(GPIO_TypeDef *hgpio, uint8_t gpio_pin) {
  // Find the handler index for this pin
  uint8_t index = 0;
  while (index < EXTI_MAX_GPIO1_PIN_NUM &&
         s_exti_gpio1_handler_configs[index].callback != NULL &&
         s_exti_gpio1_handler_configs[index].gpio_pin != gpio_pin) {
    index++;
  }
  if (index >= EXTI_MAX_GPIO1_PIN_NUM) {
    // Handler not found
    return;
  }
  // Clear the callback and index
  s_exti_gpio1_handler_configs[index].callback = NULL;
  s_exti_gpio1_handler_configs[index].gpio_pin = 0;
}

void exti_configure_pin(ExtiConfig cfg, ExtiTrigger trigger, ExtiHandlerCallback cb) {
  prv_insert_handler(cfg.peripheral, cfg.gpio_pin, cb);

  uint16_t offset;
  GPIO_TypeDef *gpiox = prv_gpio_get_instance(cfg.peripheral, cfg.gpio_pin, &offset);

  switch (trigger) {
    case ExtiTrigger_Rising:
      gpiox->ITSR |= (1UL << offset);
      gpiox->IPHSR = (1UL << offset);
      gpiox->IPLCR = (1UL << offset);
      break;
    case ExtiTrigger_Falling:
      gpiox->ITSR |= (1UL << offset);
      gpiox->IPHCR = (1UL << offset);
      gpiox->IPLSR = (1UL << offset);
      break;
    case ExtiTrigger_RisingFalling:
      gpiox->ITSR |= (1UL << offset);
      gpiox->IPHSR = (1UL << offset);
      gpiox->IPLSR = (1UL << offset);
      break;
  }

  // Configure NVIC once during pin setup
  HAL_NVIC_SetPriority(GPIO1_IRQn, 6, 0);
  HAL_NVIC_EnableIRQ(GPIO1_IRQn);
}

void exti_enable(ExtiConfig cfg) {
  uint16_t offset;
  GPIO_TypeDef *gpiox = prv_gpio_get_instance(cfg.peripheral, cfg.gpio_pin, &offset);
  // Enable the EXTI line for GPIO1
  gpiox->IESR = (1 << offset);
  // Note: NVIC is configured once in exti_configure_pin, no need to set it here
}

void exti_disable(ExtiConfig cfg) {
  uint16_t offset;
  GPIO_TypeDef *gpiox = prv_gpio_get_instance(cfg.peripheral, cfg.gpio_pin, &offset);
  // Disable the EXTI line for GPIO1
  gpiox->IECR = (1 << offset);
  gpiox->ISR = (1 << offset);
}

void HAL_GPIO_EXTI_Callback(GPIO_TypeDef *hgpio, uint16_t GPIO_Pin) {
  int index = 0;
  ExtiHandlerCallback cb = NULL;
  if (hgpio == hwp_gpio1) {
    while (index < EXTI_MAX_GPIO1_PIN_NUM && s_exti_gpio1_handler_configs[index].callback != NULL) {
      if (s_exti_gpio1_handler_configs[index].gpio_pin == GPIO_Pin) {
        cb = s_exti_gpio1_handler_configs[index].callback;
        break;
      }
      index++;
    }
  }

  if (cb != NULL) {
    bool should_context_switch = false;
    cb(&should_context_switch);
    if (should_context_switch) {
      portEND_SWITCHING_ISR(should_context_switch);
    }
  }
}

void GPIO1_IRQHandler(void) {
  // Optimized interrupt handler to avoid looping through all 78 pins
  // which causes an interrupt storm and blocks other tasks (e.g. I2C).
  GPIO_TypeDef *base = hwp_gpio1;
  // GPIO1 has pins 0-78, spanning 3 banks (32 pins each)
  for (int i = 0; i < 3; i++) {
    GPIO_TypeDef *gpiox = base + i;
    uint32_t isr = gpiox->ISR;
    uint32_t ier = gpiox->IER;
    uint32_t pending = isr & ier;

    while (pending) {
      uint32_t bit = __builtin_ctz(pending);
      uint32_t pin = (i * 32) + bit;

      // Always call the HAL handler which will clear ISR and invoke callback
      HAL_GPIO_EXTI_IRQHandler(hwp_gpio1, pin);

      pending &= ~(1UL << bit);
    }
  }
}

void exti_configure_other(ExtiLineOther exti_line, ExtiTrigger trigger) {}

void exti_enable_other(ExtiLineOther exti_line) {}

void exti_disable_other(ExtiLineOther exti_line) {}

void exti_set_pending(ExtiConfig cfg) {}

void exti_clear_pending_other(ExtiLineOther exti_line) {}
