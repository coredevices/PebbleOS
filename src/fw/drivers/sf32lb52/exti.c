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

#include "drivers/exti.h"

#include <stdbool.h>

#include "board/board.h"
#include "drivers/periph_config.h"
#include "kernel/events.h"
#include "mcu/interrupts.h"
#include "system/passert.h"

#define EXTI_MAX_GPIO1_PIN_NUM 8
#define EXTI_MAX_GPIO2_PIN_NUM 1

typedef struct {
  uint32_t gpio_pin;
  ExtiHandlerCallback callback;
} ExtiHandlerConfig_t;

static ExtiHandlerConfig_t s_exti_gpio1_handler_configs[EXTI_MAX_GPIO1_PIN_NUM];
static ExtiHandlerConfig_t s_exti_gpio2_handler_configs[EXTI_MAX_GPIO2_PIN_NUM];

static GPIO_TypeDef *prv_gpio_get_instance(GPIO_TypeDef *hgpio, uint16_t gpio_pin,
                                           uint16_t *offset) {
  uint16_t max_num;
  uint16_t inst_idx;
  GPIO_TypeDef *gpiox;

  if ((GPIO_TypeDef *)hwp_gpio1 == hgpio) {
    max_num = GPIO1_PIN_NUM;
  } else {
    max_num = GPIO2_PIN_NUM;
  }

  HAL_ASSERT(gpio_pin < max_num);

  if (gpio_pin >= max_num) {
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
  while (index < (hgpio == hwp_gpio1 ? EXTI_MAX_GPIO1_PIN_NUM : EXTI_MAX_GPIO2_PIN_NUM) &&
         s_exti_gpio1_handler_configs[index].callback != NULL) {
    index++;
  }
  if (index >= (hgpio == hwp_gpio1 ? EXTI_MAX_GPIO1_PIN_NUM : EXTI_MAX_GPIO2_PIN_NUM)) {
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
  while (index < (hgpio == hwp_gpio1 ? EXTI_MAX_GPIO1_PIN_NUM : EXTI_MAX_GPIO2_PIN_NUM) &&
         s_exti_gpio1_handler_configs[index].callback != NULL &&
         s_exti_gpio1_handler_configs[index].gpio_pin != gpio_pin) {
    index++;
  }
  if (index >= (hgpio == hwp_gpio1 ? EXTI_MAX_GPIO1_PIN_NUM : EXTI_MAX_GPIO2_PIN_NUM)) {
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
}

void exti_enable(ExtiConfig cfg) {
  uint16_t offset;
  GPIO_TypeDef *gpiox = prv_gpio_get_instance(cfg.peripheral, cfg.gpio_pin, &offset);
  if (cfg.peripheral == hwp_gpio1) {
    // Enable the EXTI line for GPIO1
    gpiox->IESR |= (1 << offset);
  } else {
    gpiox->IESR_EXT |= (1 << offset);
  }

  HAL_NVIC_SetPriority(GPIO1_IRQn, 6, 0);
  HAL_NVIC_EnableIRQ(GPIO1_IRQn);
}

void exti_disable(ExtiConfig cfg) {
  uint16_t offset;
  GPIO_TypeDef *gpiox = prv_gpio_get_instance(cfg.peripheral, cfg.gpio_pin, &offset);
  if (cfg.peripheral == hwp_gpio1) {
    // Disable the EXTI line for GPIO1
    gpiox->IECR |= (1 << offset);
  } else {
    gpiox->IECR_EXT |= (1 << offset);
  }
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

void GPIO1_IRQHandler(void) { HAL_GPIO_IRQHandler(hwp_gpio1); }

void GPIO2_IRQHandler(
    void)  // Define the interrupt siervice routine (ISR) according to the interrupt vector table
{
  HAL_GPIO_IRQHandler(hwp_gpio2);
}

void exti_configure_other(ExtiLineOther exti_line, ExtiTrigger trigger) {}

void exti_enable_other(ExtiLineOther exti_line) {}

void exti_disable_other(ExtiLineOther exti_line) {}

void exti_set_pending(ExtiConfig cfg) {}

void exti_clear_pending_other(ExtiLineOther exti_line) {}
