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

#define GPIO1_NUM_PINS 45U

#define AON_PULL_FIRST_PIN 34U
#define AON_PULL_LAST_PIN  44U
#define AON_PULL_PAWK_BASE 28U
#define PBR_PULL_FIRST_PIN 24U
#define PBR_PULL_LAST_PIN  27U
#define WAKEUP_FIRST_PIN   24U
#define WAKEUP_LAST_PIN    44U

static const struct pbl_gpio_sf32lb *prv_port(const struct pbl_gpio_port *port) {
  return container_of(port, const struct pbl_gpio_sf32lb, port);
}

static const struct pbl_gpio_sf32lb *prv_intc_port(const struct pbl_intc *intc) {
  return container_of(intc, const struct pbl_gpio_sf32lb, intc);
}

static inline uint32_t prv_getreg(const struct pbl_gpio_sf32lb *gpio, uint32_t pin,
                                  uint32_t offset) {
  return pbl_readl(gpio->base + SF32LB52_GPIO_BANK_OFFSET(pin / 32U) + offset);
}

static inline void prv_putreg(const struct pbl_gpio_sf32lb *gpio, uint32_t pin, uint32_t offset) {
  pbl_writel(GPIO_PIN_BIT(pin), gpio->base + SF32LB52_GPIO_BANK_OFFSET(pin / 32U) + offset);
}

static bool prv_is_open_drain(const struct pbl_gpio_sf32lb *gpio, uint32_t pin) {
  return (gpio->state->open_drain[pin / 32U] & GPIO_PIN_BIT(pin)) != 0U;
}

static void prv_pad_select_gpio(uint32_t pin) {
  pbl_modifyl(PINMUX_PAD_FSEL_MASK,
              PBL_FIELD_PREP(PINMUX_PAD_FSEL_MASK, PINMUX_PAD_FSEL_GPIO) | PINMUX_PAD_IE,
              SF32LB52_PINMUX1_BASE + SF32LB52_PINMUX_PAD_PA_OFFSET(pin));
}

// Pulls of always-on and backup pads are also set in the RTC block
static void prv_pad_set_pull(uint32_t pin, uint32_t flags) {
  bool pe = (flags & (PBL_GPIO_PULL_UP | PBL_GPIO_PULL_DOWN)) != 0U;
  bool ps = (flags & PBL_GPIO_PULL_UP) != 0U;

  pbl_modifyl(PINMUX_PAD_PE | PINMUX_PAD_PS, (pe ? PINMUX_PAD_PE : 0U) | (ps ? PINMUX_PAD_PS : 0U),
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

// Open drain is emulated: DOR low, level trigger with both polarities, DOE drives low
static void prv_set_open_drain(const struct pbl_gpio_sf32lb *gpio, uint32_t pin, bool enable) {
  if (enable == prv_is_open_drain(gpio, pin)) {
    return;
  }

  if (enable) {
    prv_putreg(gpio, pin, SF32LB52_GPIO_DOCR_OFFSET);
    prv_putreg(gpio, pin, SF32LB52_GPIO_ITCR_OFFSET);
    prv_putreg(gpio, pin, SF32LB52_GPIO_IPHSR_OFFSET);
    prv_putreg(gpio, pin, SF32LB52_GPIO_IPLSR_OFFSET);
    gpio->state->open_drain[pin / 32U] |= GPIO_PIN_BIT(pin);
  } else {
    prv_putreg(gpio, pin, SF32LB52_GPIO_IPHCR_OFFSET);
    prv_putreg(gpio, pin, SF32LB52_GPIO_IPLCR_OFFSET);
    gpio->state->open_drain[pin / 32U] &= ~GPIO_PIN_BIT(pin);
  }
}

static int prv_configure(const struct pbl_gpio_port *port, uint8_t pin, uint32_t flags) {
  const struct pbl_gpio_sf32lb *gpio = prv_port(port);
  bool open_drain =
      (flags & (PBL_GPIO_OUTPUT | PBL_GPIO_OPEN_DRAIN)) == (PBL_GPIO_OUTPUT | PBL_GPIO_OPEN_DRAIN);

  if ((flags & (PBL_GPIO_INPUT | PBL_GPIO_OUTPUT)) == 0U) {
    return -EINVAL;
  }

  pbl_irq_lock();

  // The trigger registers emulate open drain
  if (open_drain && gpio->intc.lines[pin].handler != nullptr) {
    pbl_irq_unlock();
    return -EBUSY;
  }

  prv_pad_set_pull(pin, flags);
  prv_pad_select_gpio(pin);
  prv_set_open_drain(gpio, pin, open_drain);

  if (open_drain) {
    if (flags & PBL_GPIO_OUTPUT_INIT_HIGH) {
      prv_putreg(gpio, pin, SF32LB52_GPIO_DOECR_OFFSET);
    } else if (flags & PBL_GPIO_OUTPUT_INIT_LOW) {
      prv_putreg(gpio, pin, SF32LB52_GPIO_DOESR_OFFSET);
    }
  } else if (flags & PBL_GPIO_OUTPUT) {
    if (flags & PBL_GPIO_OUTPUT_INIT_HIGH) {
      prv_putreg(gpio, pin, SF32LB52_GPIO_DOSR_OFFSET);
    } else if (flags & PBL_GPIO_OUTPUT_INIT_LOW) {
      prv_putreg(gpio, pin, SF32LB52_GPIO_DOCR_OFFSET);
    }
    prv_putreg(gpio, pin, SF32LB52_GPIO_DOESR_OFFSET);
  } else {
    prv_putreg(gpio, pin, SF32LB52_GPIO_DOECR_OFFSET);
  }

  pbl_irq_unlock();

  return 0;
}

static int prv_get(const struct pbl_gpio_port *port, uint8_t pin) {
  const struct pbl_gpio_sf32lb *gpio = prv_port(port);
  uint32_t bit = GPIO_PIN_BIT(pin);

  if (prv_is_open_drain(gpio, pin)) {
    return (prv_getreg(gpio, pin, SF32LB52_GPIO_DOER_OFFSET) & bit) == 0U;
  }

  if ((prv_getreg(gpio, pin, SF32LB52_GPIO_DOER_OFFSET) & bit) != 0U) {
    return (prv_getreg(gpio, pin, SF32LB52_GPIO_DOR_OFFSET) & bit) != 0U;
  }

  return (prv_getreg(gpio, pin, SF32LB52_GPIO_DIR_OFFSET) & bit) != 0U;
}

static int prv_set(const struct pbl_gpio_port *port, uint8_t pin, bool level) {
  const struct pbl_gpio_sf32lb *gpio = prv_port(port);

  if (prv_is_open_drain(gpio, pin)) {
    prv_putreg(gpio, pin, level ? SF32LB52_GPIO_DOECR_OFFSET : SF32LB52_GPIO_DOESR_OFFSET);
  } else {
    prv_putreg(gpio, pin, level ? SF32LB52_GPIO_DOSR_OFFSET : SF32LB52_GPIO_DOCR_OFFSET);
  }

  return 0;
}

static const struct pbl_gpio_port_ops s_port_ops = {
  .configure = prv_configure,
  .get = prv_get,
  .set = prv_set,
};

static int prv_intc_configure(const struct pbl_intc *intc, uint32_t hwirq, uint32_t type) {
  const struct pbl_gpio_sf32lb *gpio = prv_intc_port(intc);
  bool high = (type & (PBL_IRQ_TYPE_EDGE_RISING | PBL_IRQ_TYPE_LEVEL_HIGH)) != 0U;
  bool low = (type & (PBL_IRQ_TYPE_EDGE_FALLING | PBL_IRQ_TYPE_LEVEL_LOW)) != 0U;

  if (prv_is_open_drain(gpio, hwirq)) {
    return -EBUSY;
  }

  prv_pad_select_gpio(hwirq);
  prv_putreg(gpio, hwirq, SF32LB52_GPIO_DOECR_OFFSET);

  prv_putreg(
      gpio, hwirq,
      (type & PBL_IRQ_TYPE_EDGE_BOTH) ? SF32LB52_GPIO_ITSR_OFFSET : SF32LB52_GPIO_ITCR_OFFSET);
  prv_putreg(gpio, hwirq, high ? SF32LB52_GPIO_IPHSR_OFFSET : SF32LB52_GPIO_IPHCR_OFFSET);
  prv_putreg(gpio, hwirq, low ? SF32LB52_GPIO_IPLSR_OFFSET : SF32LB52_GPIO_IPLCR_OFFSET);
  prv_putreg(gpio, hwirq, SF32LB52_GPIO_ISR_OFFSET);

  return 0;
}

static int prv_intc_enable(const struct pbl_intc *intc, uint32_t hwirq) {
  prv_putreg(prv_intc_port(intc), hwirq, SF32LB52_GPIO_IESR_OFFSET);

  return 0;
}

static int prv_intc_disable(const struct pbl_intc *intc, uint32_t hwirq) {
  const struct pbl_gpio_sf32lb *gpio = prv_intc_port(intc);

  prv_putreg(gpio, hwirq, SF32LB52_GPIO_IECR_OFFSET);
  prv_putreg(gpio, hwirq, SF32LB52_GPIO_ISR_OFFSET);

  return 0;
}

static const struct pbl_intc_ops s_intc_ops = {
  .configure = prv_intc_configure,
  .enable = prv_intc_enable,
  .disable = prv_intc_disable,
};

static void prv_irq_handler(const struct pbl_gpio_sf32lb *gpio) {
  for (uint32_t bank = 0U; bank * 32U < GPIO1_NUM_PINS; bank++) {
    uint32_t pin0 = bank * 32U;
    uint32_t pending = prv_getreg(gpio, pin0, SF32LB52_GPIO_ISR_OFFSET) &
                       prv_getreg(gpio, pin0, SF32LB52_GPIO_IER_OFFSET);

    while (pending != 0U) {
      uint32_t pin = pin0 + (uint32_t)__builtin_ctz(pending);

      pending &= pending - 1U;
      prv_putreg(gpio, pin, SF32LB52_GPIO_ISR_OFFSET);

      // Wakeup pins latch their edges in the HPAON block, even while awake
      if (pin >= WAKEUP_FIRST_PIN && pin <= WAKEUP_LAST_PIN) {
        pbl_writel(HPSYS_AON_WCR_PIN(pin - WAKEUP_FIRST_PIN) | HPSYS_AON_WCR_AON,
                   SF32LB52_HPSYS_AON_BASE + SF32LB52_HPSYS_AON_WCR_OFFSET);
      }

      pbl_intc_dispatch(&gpio->intc, pin);
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
