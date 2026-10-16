/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/clk/sf32lb.h>
#include <pbl/drivers/gpio/sf32lb.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/mmio.h>
#include <pbl/util/misc.h>

#include <hardware/sf32lb52_gpio.h>
#include <hardware/sf32lb52_hpsys_aon.h>
#include <hardware/sf32lb52_pinmux.h>
#include <hardware/sf32lb52_rtc.h>

#define GPIO1_NUM_PINS  45U
#define GPIO1_NUM_BANKS 2U

#define AON_PULL_FIRST_PIN 34U
#define AON_PULL_LAST_PIN  44U
#define AON_PULL_PAWK_BASE 28U
#define PBR_PULL_FIRST_PIN 24U
#define PBR_PULL_LAST_PIN  27U
#define WAKEUP_FIRST_PIN   24U

static const struct pbl_gpio_sf32lb *prv_port(const struct pbl_gpio_port *port) {
  return container_of(port, const struct pbl_gpio_sf32lb, port);
}

static const struct pbl_gpio_sf32lb *prv_intc_port(const struct pbl_intc *intc) {
  return container_of(intc, const struct pbl_gpio_sf32lb, intc);
}

static inline uint32_t prv_getreg(const struct pbl_gpio_sf32lb *gpio, uint32_t bank,
                                  uint32_t offset) {
  return pbl_readl(gpio->base + SF32LB52_GPIO_BANK_OFFSET(bank) + offset);
}

static inline void prv_putreg(const struct pbl_gpio_sf32lb *gpio, uint32_t bank, uint32_t offset,
                              uint32_t value) {
  pbl_writel(value, gpio->base + SF32LB52_GPIO_BANK_OFFSET(bank) + offset);
}

static inline void prv_putpin(const struct pbl_gpio_sf32lb *gpio, uint32_t pin, uint32_t offset) {
  prv_putreg(gpio, pin / 32U, offset, GPIO_PIN_BIT(pin));
}

// Pulls of always-on and backup pads are also set in the RTC block
static void prv_pad_configure(uint32_t pin, uint32_t flags) {
  bool pe = (flags & (PBL_GPIO_PULL_UP | PBL_GPIO_PULL_DOWN)) != 0U;
  bool ps = (flags & PBL_GPIO_PULL_UP) != 0U;
  uint32_t val = PBL_FIELD_PREP(PINMUX_PAD_FSEL_MASK, PINMUX_PAD_FSEL_GPIO) | PINMUX_PAD_IE;

  if (pe) {
    val |= PINMUX_PAD_PE;
  }
  if (ps) {
    val |= PINMUX_PAD_PS;
  }

  pbl_modifyl(PINMUX_PAD_FSEL_MASK | PINMUX_PAD_PE | PINMUX_PAD_PS | PINMUX_PAD_IE, val,
              SF32LB52_PINMUX1_BASE + SF32LB52_PINMUX_PAD_PA_OFFSET(pin));

  if (pin >= AON_PULL_FIRST_PIN && pin <= AON_PULL_LAST_PIN) {
    uint32_t bit = RTC_PAWK_PIN(pin - AON_PULL_PAWK_BASE);

    pbl_modifyl(bit, pe ? bit : 0U, SF32LB52_RTC_BASE + SF32LB52_RTC_PAWK1R_OFFSET);
    pbl_modifyl(bit, ps ? bit : 0U, SF32LB52_RTC_BASE + SF32LB52_RTC_PAWK2R_OFFSET);
  } else if (pin >= PBR_PULL_FIRST_PIN && pin <= PBR_PULL_LAST_PIN) {
    pbl_modifyl(RTC_PBR_PE | RTC_PBR_PS, (pe ? RTC_PBR_PE : 0U) | (ps ? RTC_PBR_PS : 0U),
                SF32LB52_RTC_BASE + SF32LB52_RTC_PBR_OFFSET(pin - PBR_PULL_FIRST_PIN));
  }
}

static int prv_configure(const struct pbl_gpio_port *port, uint8_t pin, uint32_t flags) {
  const struct pbl_gpio_sf32lb *gpio = prv_port(port);
  uint32_t bank = pin / 32U;
  uint32_t bit = GPIO_PIN_BIT(pin);

  if ((flags & (PBL_GPIO_INPUT | PBL_GPIO_OUTPUT)) == 0U) {
    return -EINVAL;
  }

  pbl_irq_lock();

  if ((flags & PBL_GPIO_OUTPUT) == 0U) {
    gpio->state->od[bank] &= ~bit;
    prv_putpin(gpio, pin, SF32LB52_GPIO_DOECR_OFFSET);
  } else if ((flags & PBL_GPIO_OPEN_DRAIN) != 0U) {
    // Open drain: output low, driven by enabling the output
    gpio->state->od[bank] |= bit;
    prv_putpin(gpio, pin, SF32LB52_GPIO_DOCR_OFFSET);
    if ((flags & PBL_GPIO_OUTPUT_INIT_HIGH) != 0U) {
      prv_putpin(gpio, pin, SF32LB52_GPIO_DOECR_OFFSET);
    } else if ((flags & PBL_GPIO_OUTPUT_INIT_LOW) != 0U) {
      prv_putpin(gpio, pin, SF32LB52_GPIO_DOESR_OFFSET);
    }
  } else {
    gpio->state->od[bank] &= ~bit;
    if ((flags & PBL_GPIO_OUTPUT_INIT_HIGH) != 0U) {
      prv_putpin(gpio, pin, SF32LB52_GPIO_DOSR_OFFSET);
    } else if ((flags & PBL_GPIO_OUTPUT_INIT_LOW) != 0U) {
      prv_putpin(gpio, pin, SF32LB52_GPIO_DOCR_OFFSET);
    }
    prv_putpin(gpio, pin, SF32LB52_GPIO_DOESR_OFFSET);
  }

  prv_pad_configure(pin, flags);

  pbl_irq_unlock();

  return 0;
}

static int prv_get(const struct pbl_gpio_port *port, uint8_t pin) {
  const struct pbl_gpio_sf32lb *gpio = prv_port(port);

  return (prv_getreg(gpio, pin / 32U, SF32LB52_GPIO_DIR_OFFSET) & GPIO_PIN_BIT(pin)) != 0U;
}

static int prv_set(const struct pbl_gpio_port *port, uint8_t pin, bool level) {
  const struct pbl_gpio_sf32lb *gpio = prv_port(port);

  if ((gpio->state->od[pin / 32U] & GPIO_PIN_BIT(pin)) != 0U) {
    prv_putpin(gpio, pin, level ? SF32LB52_GPIO_DOECR_OFFSET : SF32LB52_GPIO_DOESR_OFFSET);
  } else {
    prv_putpin(gpio, pin, level ? SF32LB52_GPIO_DOSR_OFFSET : SF32LB52_GPIO_DOCR_OFFSET);
  }

  return 0;
}

static const struct pbl_gpio_port_ops s_port_ops = {
  .configure = prv_configure,
  .get = prv_get,
  .set = prv_set,
};

// The line takes the pin as an input, keeping its pulls
static int prv_intc_configure(const struct pbl_intc *intc, uint32_t hwirq, uint32_t type) {
  const struct pbl_gpio_sf32lb *gpio = prv_intc_port(intc);
  bool edge = (type & PBL_IRQ_TYPE_EDGE_BOTH) != 0U;
  bool high = (type & (PBL_IRQ_TYPE_EDGE_RISING | PBL_IRQ_TYPE_LEVEL_HIGH)) != 0U;
  bool low = (type & (PBL_IRQ_TYPE_EDGE_FALLING | PBL_IRQ_TYPE_LEVEL_LOW)) != 0U;

  gpio->state->od[hwirq / 32U] &= ~GPIO_PIN_BIT(hwirq);
  prv_putpin(gpio, hwirq, SF32LB52_GPIO_DOECR_OFFSET);
  pbl_modifyl(PINMUX_PAD_FSEL_MASK,
              PBL_FIELD_PREP(PINMUX_PAD_FSEL_MASK, PINMUX_PAD_FSEL_GPIO) | PINMUX_PAD_IE,
              SF32LB52_PINMUX1_BASE + SF32LB52_PINMUX_PAD_PA_OFFSET(hwirq));

  prv_putpin(gpio, hwirq, edge ? SF32LB52_GPIO_ITSR_OFFSET : SF32LB52_GPIO_ITCR_OFFSET);
  prv_putpin(gpio, hwirq, high ? SF32LB52_GPIO_IPHSR_OFFSET : SF32LB52_GPIO_IPHCR_OFFSET);
  prv_putpin(gpio, hwirq, low ? SF32LB52_GPIO_IPLSR_OFFSET : SF32LB52_GPIO_IPLCR_OFFSET);
  prv_putpin(gpio, hwirq, SF32LB52_GPIO_ISR_OFFSET);

  return 0;
}

static int prv_intc_enable(const struct pbl_intc *intc, uint32_t hwirq) {
  prv_putpin(prv_intc_port(intc), hwirq, SF32LB52_GPIO_IESR_OFFSET);

  return 0;
}

static int prv_intc_disable(const struct pbl_intc *intc, uint32_t hwirq) {
  const struct pbl_gpio_sf32lb *gpio = prv_intc_port(intc);

  prv_putpin(gpio, hwirq, SF32LB52_GPIO_IECR_OFFSET);
  prv_putpin(gpio, hwirq, SF32LB52_GPIO_ISR_OFFSET);

  return 0;
}

static const struct pbl_intc_ops s_intc_ops = {
  .configure = prv_intc_configure,
  .enable = prv_intc_enable,
  .disable = prv_intc_disable,
};

// Wakeup pins also latch in HPAON, which the AON handler leaves for us to clear
static void prv_clear_wakeup(uint32_t bank, uint32_t pending) {
  uint64_t pins = ((uint64_t)pending << (bank * 32U)) >> WAKEUP_FIRST_PIN;

  if ((uint32_t)pins != 0U) {
    pbl_writel(((uint32_t)pins << 8) | HPSYS_AON_WCR_AON,
               SF32LB52_HPSYS_AON_BASE + SF32LB52_HPSYS_AON_WCR_OFFSET);
  }
}

static void prv_irq_handler(const struct pbl_gpio_sf32lb *gpio) {
  for (uint32_t bank = 0U; bank < GPIO1_NUM_BANKS; bank++) {
    uint32_t pending = prv_getreg(gpio, bank, SF32LB52_GPIO_ISR_OFFSET) &
                       prv_getreg(gpio, bank, SF32LB52_GPIO_IER_OFFSET);

    if (pending == 0U) {
      continue;
    }

    prv_putreg(gpio, bank, SF32LB52_GPIO_ISR_OFFSET, pending);
    prv_clear_wakeup(bank, pending);

    while (pending != 0U) {
      pbl_intc_dispatch(&gpio->intc, bank * 32U + (uint32_t)__builtin_ctz(pending));
      pending &= pending - 1U;
    }
  }
}

static int prv_init(const struct pbl_device *dev) {
  const struct pbl_gpio_sf32lb *gpio = prv_port(container_of(dev, const struct pbl_gpio_port, dev));
  int ret;

  ret = pbl_device_init(&gpio->clk.ctrl->dev);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_clk_enable(&gpio->clk);
  if (ret != 0) {
    return ret;
  }

  pbl_irq_enable(PBL_IRQN(GPIO1));

  return 0;
}

static struct pbl_gpio_sf32lb_state s_gpio1_state;

PBL_DEVICE_STATE_DEFINE(pbl_gpio_sf32lb_gpio1);
PBL_INTC_STATE_DEFINE(pbl_gpio_sf32lb_gpio1_intc, GPIO1_NUM_PINS);
const struct pbl_gpio_sf32lb pbl_gpio_sf32lb_gpio1 = {
  .port = PBL_GPIO_PORT_INIT(pbl_gpio_sf32lb_gpio1, "gpio1", prv_init, nullptr, &s_port_ops,
                             GPIO1_NUM_PINS, &pbl_gpio_sf32lb_gpio1.intc),
  .intc = PBL_INTC_INIT(pbl_gpio_sf32lb_gpio1_intc, "gpio1_intc", nullptr,
                        &pbl_gpio_sf32lb_gpio1.port.dev, &s_intc_ops),
  .base = SF32LB52_GPIO1_BASE,
  .clk = PBL_CLK(SF32LB_HPSYS_RCC, SF32LB52_HPSYS_CLK_GPIO1),
  .state = &s_gpio1_state,
};
PBL_DEVICE_REGISTER(pbl_gpio_sf32lb_gpio1, &pbl_gpio_sf32lb_gpio1.port.dev);
PBL_DEVICE_REGISTER(pbl_gpio_sf32lb_gpio1_intc, &pbl_gpio_sf32lb_gpio1.intc.dev);

PBL_IRQ_CONNECT(GPIO1, 6, prv_irq_handler, &pbl_gpio_sf32lb_gpio1, 0);
