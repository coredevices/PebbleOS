/* SPDX-FileCopyrightText: 2025 SiFli Technologies(Nanjing) Co., Ltd */
/* SPDX-License-Identifier: Apache-2.0 */

#include <inttypes.h>

#include <pbl/drivers/exti.h>
#include <pbl/drivers/gpio/sf32lb.h>
#include <pbl/logging/logging.h>
#include <pbl/util/size.h>

#include <board/board.h>
#include <system/passert.h>

PBL_LOG_MODULE_DEFINE(driver_exti_sf32lb, CONFIG_DRIVER_EXTI_LOG_LEVEL);

static ExtiHandlerCallback s_callbacks[64];

static void prv_handler(void *arg) {
  s_callbacks[(uintptr_t)arg]();
}

static struct pbl_irq_spec prv_spec(ExtiConfig cfg, ExtiTrigger trigger) {
  uint8_t type = (trigger == ExtiTrigger_Rising)    ? PBL_IRQ_TYPE_EDGE_RISING
                 : (trigger == ExtiTrigger_Falling) ? PBL_IRQ_TYPE_EDGE_FALLING
                                                    : PBL_IRQ_TYPE_EDGE_BOTH;

  return (struct pbl_irq_spec)PBL_IRQ_SPEC(SF32LB_GPIO1_INTC, cfg.gpio_pin, type);
}

void exti_configure_pin(ExtiConfig cfg, ExtiTrigger trigger, ExtiHandlerCallback cb) {
  const struct pbl_gpio gpio = PBL_GPIO(SF32LB_GPIO1, cfg.gpio_pin,
                                        (cfg.pull == GPIO_PuPd_UP)     ? PBL_GPIO_PULL_UP
                                        : (cfg.pull == GPIO_PuPd_DOWN) ? PBL_GPIO_PULL_DOWN
                                                                       : 0U);
  const struct pbl_irq_spec spec = prv_spec(cfg, trigger);
  int ret;

  PBL_ASSERTN(cfg.gpio_pin < ARRAY_LENGTH(s_callbacks));
  PBL_ASSERTN(pbl_device_init(&SF32LB_GPIO1_INTC->dev) == 0);

  (void)pbl_intc_free(&spec);

  ret = pbl_gpio_configure(&gpio, PBL_GPIO_INPUT);
  if (ret == 0) {
    s_callbacks[cfg.gpio_pin] = cb;
    ret = pbl_intc_request(&spec, prv_handler, (void *)(uintptr_t)cfg.gpio_pin);
  }

  if (ret != 0) {
    PBL_LOG_ERR("Could not configure pin %" PRIu32 " (%d)", cfg.gpio_pin, ret);
  }
}

void exti_enable(ExtiConfig cfg) {
  const struct pbl_irq_spec spec = prv_spec(cfg, ExtiTrigger_Rising);

  (void)pbl_intc_enable(&spec);
}

void exti_disable(ExtiConfig cfg) {
  const struct pbl_irq_spec spec = prv_spec(cfg, ExtiTrigger_Rising);

  (void)pbl_intc_disable(&spec);
}
