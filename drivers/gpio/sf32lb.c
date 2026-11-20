/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/clk/sf32lb.h>
#include <pbl/drivers/gpio/sf32lb.h>
#ifdef CONFIG_PINCTRL_SF32LB52
#include <pbl/drivers/pinctrl/sf32lb52.h>
#endif
#ifdef CONFIG_PINCTRL_SF32LB57
#include <pbl/drivers/pinctrl/sf32lb57.h>
#endif
#include <pbl/kernel/compiler.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/mmio.h>
#include <pbl/kernel/reboot.h>
#include <pbl/util/misc.h>

#include <hardware/sf32lb52_gpio.h>
#include <hardware/sf32lb52_hpsys_aon.h>
#ifdef CONFIG_PINCTRL_SF32LB52
#include <hardware/sf32lb52_pmuc.h>
#endif
#ifdef CONFIG_PINCTRL_SF32LB57
#include <hardware/sf32lb57_pmuc.h>
#endif

// PA pads, as the pin controller of the series takes them
struct pbl_gpio_sf32lb_pads {
  // GPIO function, input buffer and the pulls of the flags
  void (*configure)(uint32_t pin, uint32_t flags);
  // GPIO function and input buffer, keeping the pulls
  void (*input)(uint32_t pin);
};

#ifdef CONFIG_PINCTRL_SF32LB52
#define WAKEUP_FIRST_PIN 24U

// Hibernate wakes on two PMU pins, each routed from one of PA24-PA44
#define PMU_NUM_WAKE_LINES 21U
#define PMU_MAX_WAKE_LINES 2U

#define SF32LB52_PAD_GPIO(pin) \
  PBL_PINCTRL_SF32LB52_PIN(PBL_PINCTRL_SF32LB52_PORT_PA, pin, 0U, 0U, 0U)
#define SF32LB52_PAD_MSK                                                               \
  (PBL_PINCTRL_SF32LB52_FSEL_MSK | PBL_PINCTRL_SF32LB52_PE | PBL_PINCTRL_SF32LB52_PS | \
   PBL_PINCTRL_SF32LB52_IE)

static void prv_sf32lb52_configure(uint32_t pin, uint32_t flags) {
  uint32_t val = SF32LB52_PAD_GPIO(pin) | PBL_PINCTRL_SF32LB52_INPUT;

  if ((flags & PBL_GPIO_PULL_UP) != 0U) {
    val |= PBL_PINCTRL_SF32LB52_PULL_UP;
  } else if ((flags & PBL_GPIO_PULL_DOWN) != 0U) {
    val |= PBL_PINCTRL_SF32LB52_PULL_DOWN;
  }

  (void)pbl_pinctrl_sf32lb52_modify(val, SF32LB52_PAD_MSK);
}

static void prv_sf32lb52_input(uint32_t pin) {
  (void)pbl_pinctrl_sf32lb52_modify(SF32LB52_PAD_GPIO(pin) | PBL_PINCTRL_SF32LB52_INPUT,
                                    PBL_PINCTRL_SF32LB52_FSEL_MSK | PBL_PINCTRL_SF32LB52_IE);
}

static const struct pbl_gpio_sf32lb_pads s_sf32lb52_pads = {
  .configure = prv_sf32lb52_configure,
  .input = prv_sf32lb52_input,
};

static int prv_pmu_line(uint32_t pin) {
  if (pin < WAKEUP_FIRST_PIN || pin >= WAKEUP_FIRST_PIN + PMU_NUM_WAKE_LINES) {
    return -1;
  }

  return (int)(pin - WAKEUP_FIRST_PIN);
}

static uint32_t prv_pmu_line_pin(uint32_t line) {
  return WAKEUP_FIRST_PIN + line;
}

static void prv_pmu_arm(uintptr_t pmuc, uint32_t lines, uint32_t high) {
  uint32_t clear = 0U;
  uint32_t set = 0U;
  uint32_t wer = 0U;
  uint32_t n = 0U;

  for (uint32_t line = 0U; line < PMU_NUM_WAKE_LINES; line++) {
    if ((lines & PBL_BIT(line)) == 0U) {
      continue;
    }

    clear |= PMUC_CR_PIN_SEL_MASK(n) | PMUC_CR_PIN_MODE_MASK(n);
    set |=
        PBL_FIELD_PREP(PMUC_CR_PIN_SEL_MASK(n), line) |
        PBL_FIELD_PREP(PMUC_CR_PIN_MODE_MASK(n),
                       (high & PBL_BIT(line)) != 0U ? PMUC_CR_PIN_MODE_LOW : PMUC_CR_PIN_MODE_HIGH);
    wer |= PMUC_WER_PIN0 << n;
    n++;
  }

  pbl_modifyl(clear, set, pmuc + SF32LB52_PMUC_CR_OFFSET);
  // A pin status may be latched from before
  pbl_writel(wer, pmuc + SF32LB52_PMUC_WCR_OFFSET);
  pbl_modifyl(0U, wer, pmuc + SF32LB52_PMUC_WER_OFFSET);
}
#endif

#ifdef CONFIG_PINCTRL_SF32LB57
#define SF32LB57_PAD_GPIO(pin) PBL_PINCTRL_SF32LB57_PIN(PBL_PINCTRL_SF32LB57_PORT_PA, pin, 0U)
#define SF32LB57_PAD_MSK                                                               \
  (PBL_PINCTRL_SF32LB57_FSEL_MSK | PBL_PINCTRL_SF32LB57_PE | PBL_PINCTRL_SF32LB57_PS | \
   PBL_PINCTRL_SF32LB57_IE)

static void prv_sf32lb57_configure(uint32_t pin, uint32_t flags) {
  uint32_t val = SF32LB57_PAD_GPIO(pin) | PBL_PINCTRL_SF32LB57_INPUT;

  if ((flags & PBL_GPIO_PULL_UP) != 0U) {
    val |= PBL_PINCTRL_SF32LB57_PULL_UP;
  } else if ((flags & PBL_GPIO_PULL_DOWN) != 0U) {
    val |= PBL_PINCTRL_SF32LB57_PULL_DOWN;
  }

  (void)pbl_pinctrl_sf32lb57_modify(val, SF32LB57_PAD_MSK);
}

static void prv_sf32lb57_input(uint32_t pin) {
  (void)pbl_pinctrl_sf32lb57_modify(SF32LB57_PAD_GPIO(pin) | PBL_PINCTRL_SF32LB57_INPUT,
                                    PBL_PINCTRL_SF32LB57_FSEL_MSK | PBL_PINCTRL_SF32LB57_IE);
}

static const struct pbl_gpio_sf32lb_pads s_sf32lb57_pads = {
  .configure = prv_sf32lb57_configure,
  .input = prv_sf32lb57_input,
};

// Hibernate wakes on PA33-PA42 and PA24-PA27, each with a PMU line of its own
#define PMU_NUM_WAKE_LINES 14U
#define PMU_MAX_WAKE_LINES PMU_NUM_WAKE_LINES

static int prv_pmu_line(uint32_t pin) {
  if (pin >= 33U && pin <= 42U) {
    return (int)(pin - 33U);
  }
  if (pin >= 24U && pin <= 27U) {
    return (int)(pin - 14U);
  }

  return -1;
}

static uint32_t prv_pmu_line_pin(uint32_t line) {
  return line < 10U ? 33U + line : 14U + line;
}

static void prv_pmu_arm(uintptr_t pmuc, uint32_t lines, uint32_t high) {
  uint32_t clear = 0U;
  uint32_t set = 0U;

  for (uint32_t line = 0U; line < PMU_NUM_WAKE_LINES; line++) {
    if ((lines & PBL_BIT(line)) == 0U) {
      continue;
    }

    clear |= PMUC_WKUP_MODE_MASK(line);
    set |= PBL_FIELD_PREP(PMUC_WKUP_MODE_MASK(line),
                          (high & PBL_BIT(line)) != 0U ? PMUC_WKUP_MODE_LOW : PMUC_WKUP_MODE_HIGH);
  }

  pbl_modifyl(clear, set, pmuc + SF32LB57_PMUC_WKUP_MODE_OFFSET);
  // A pin status may be latched from before
  pbl_writel(lines, pmuc + SF32LB57_PMUC_WCR_OFFSET);
  pbl_modifyl(0U, lines, pmuc + SF32LB57_PMUC_WER_OFFSET);
}
#endif

struct pbl_gpio_sf32lb_intc {
  struct pbl_intc intc;
  const struct pbl_gpio_sf32lb *port;
};

// Wake lines, one bit per PMU wake line, and their port
static const struct pbl_gpio_sf32lb *s_wake_port;
static uint32_t s_wake;

static const struct pbl_gpio_sf32lb *prv_port(const struct pbl_gpio_port *port) {
  return PBL_CONTAINER_OF(port, const struct pbl_gpio_sf32lb, port);
}

static const struct pbl_gpio_sf32lb *prv_intc_port(const struct pbl_intc *intc) {
  return PBL_CONTAINER_OF(intc, const struct pbl_gpio_sf32lb_intc, intc)->port;
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

  gpio->pads->configure(pin, flags);

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
  gpio->pads->input(hwirq);

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

static int prv_intc_set_wake(const struct pbl_intc *intc, uint32_t hwirq, bool on) {
  const struct pbl_gpio_sf32lb *gpio = prv_intc_port(intc);
  int line = prv_pmu_line(hwirq);
  uint32_t bit;

  if (gpio->pmuc == 0U || line < 0) {
    return on ? -ENOTSUP : 0;
  }

  bit = PBL_BIT((uint32_t)line);
  if (!on) {
    s_wake &= ~bit;
    return 0;
  }

  if ((s_wake & bit) == 0U && (uint32_t)PBL_POPCOUNT(s_wake) >= PMU_MAX_WAKE_LINES) {
    return -ENOSPC;
  }

  s_wake |= bit;
  s_wake_port = gpio;

  return 0;
}

static const struct pbl_intc_ops s_intc_ops = {
  .configure = prv_intc_configure,
  .enable = prv_intc_enable,
  .disable = prv_intc_disable,
  .set_wake = prv_intc_set_wake,
};

// Waking on the level opposite to the current one keeps a pin held at
// power-off from waking at once
static void prv_arm_wake(enum pbl_sys_off_mode mode, void *user_data) {
  uint32_t high = 0U;

  if (s_wake == 0U) {
    return;
  }

  for (uint32_t line = 0U; line < PMU_NUM_WAKE_LINES; line++) {
    if ((s_wake & PBL_BIT(line)) != 0U &&
        prv_get(&s_wake_port->port, (uint8_t)prv_pmu_line_pin(line)) != 0) {
      high |= PBL_BIT(line);
    }
  }

  prv_pmu_arm(s_wake_port->pmuc, s_wake, high);
}

// Last, so that power-off handlers that end the system run first
PBL_SYS_OFF_HANDLER_DEFINE(gpio_sf32lb_wake, PBL_SYS_OFF_POWER_OFF, PBL_SYS_OFF_PRIO_LOW,
                           prv_arm_wake, nullptr);

// SF32LB52 wakeup pins also latch in HPAON, which the AON handler leaves for us to clear
static void prv_clear_wakeup(const struct pbl_gpio_sf32lb *gpio, uint32_t bank, uint32_t pending) {
#ifdef CONFIG_PINCTRL_SF32LB52
  uint64_t pins = ((uint64_t)pending << (bank * 32U)) >> WAKEUP_FIRST_PIN;

  if (gpio->aon != 0U && (uint32_t)pins != 0U) {
    pbl_writel(((uint32_t)pins << 8) | HPSYS_AON_WCR_AON,
               gpio->aon + SF32LB52_HPSYS_AON_WCR_OFFSET);
  }
#endif
}

static void prv_irq_handler(const struct pbl_gpio_sf32lb *gpio) {
  for (uint32_t bank = 0U; bank * 32U < gpio->port.num_pins; bank++) {
    uint32_t pending = prv_getreg(gpio, bank, SF32LB52_GPIO_ISR_OFFSET) &
                       prv_getreg(gpio, bank, SF32LB52_GPIO_IER_OFFSET);

    if (pending == 0U) {
      continue;
    }

    prv_putreg(gpio, bank, SF32LB52_GPIO_ISR_OFFSET, pending);
    prv_clear_wakeup(gpio, bank, pending);

    while (pending != 0U) {
      pbl_intc_dispatch(gpio->port.intc, bank * 32U + (uint32_t)PBL_CTZ(pending));
      pending &= pending - 1U;
    }
  }
}

static int prv_init(const struct pbl_device *dev) {
  const struct pbl_gpio_sf32lb *gpio =
      prv_port(PBL_CONTAINER_OF(dev, const struct pbl_gpio_port, dev));
  int ret;

  ret = pbl_device_init(&gpio->clk.ctrl->dev);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_clk_enable(&gpio->clk);
  if (ret != 0) {
    return ret;
  }

  pbl_irq_enable(gpio->irq);

  return 0;
}

#define PBL_GPIO_SF32LB52_DEFINE(sym, _name, _base, _irq, _clk, _num_pins, _aon, _pmuc)           \
  extern const struct pbl_intc sym##_intc;                                                        \
  static struct pbl_gpio_sf32lb_state sym##_sf32lb_state;                                         \
  PBL_DEVICE_STATE_DEFINE(sym);                                                                   \
  PBL_INTC_STATE_DEFINE(sym##_intc, _num_pins);                                                   \
  PBL_DEVICE_INSTANCE(sym, struct pbl_gpio_sf32lb, struct pbl_gpio_port, port){                   \
    .port =                                                                                       \
        PBL_GPIO_PORT_INIT(sym, _name, prv_init, nullptr, &s_port_ops, _num_pins, &sym##_intc),   \
    .pads = &s_sf32lb52_pads,                                                                     \
    .base = (_base),                                                                              \
    .irq = (_irq),                                                                                \
    .clk = _clk,                                                                                  \
    .aon = (_aon),                                                                                \
    .pmuc = (_pmuc),                                                                              \
    .state = &sym##_sf32lb_state,                                                                 \
  };                                                                                              \
  PBL_DEVICE_INSTANCE(sym##_intc, struct pbl_gpio_sf32lb_intc, struct pbl_intc, intc){            \
    .intc = PBL_INTC_INIT(sym##_intc, _name "_intc", nullptr, &sym##_inst.port.dev, &s_intc_ops), \
    .port = &sym##_inst,                                                                          \
  };                                                                                              \
  PBL_IRQ_CONNECT_NUM(_irq, CONFIG_GPIO_SF32LB_IRQ_PRIO, prv_irq_handler, &sym##_inst, 0)

// No wakeup latches in HPAON on the SF32LB57
#define PBL_GPIO_SF32LB57_DEFINE(sym, _name, _base, _irq, _clk, _num_pins, _pmuc)                 \
  extern const struct pbl_intc sym##_intc;                                                        \
  static struct pbl_gpio_sf32lb_state sym##_sf32lb_state;                                         \
  PBL_DEVICE_STATE_DEFINE(sym);                                                                   \
  PBL_INTC_STATE_DEFINE(sym##_intc, _num_pins);                                                   \
  PBL_DEVICE_INSTANCE(sym, struct pbl_gpio_sf32lb, struct pbl_gpio_port, port){                   \
    .port =                                                                                       \
        PBL_GPIO_PORT_INIT(sym, _name, prv_init, nullptr, &s_port_ops, _num_pins, &sym##_intc),   \
    .pads = &s_sf32lb57_pads,                                                                     \
    .base = (_base),                                                                              \
    .irq = (_irq),                                                                                \
    .clk = _clk,                                                                                  \
    .aon = (0U),                                                                                  \
    .pmuc = (_pmuc),                                                                              \
    .state = &sym##_sf32lb_state,                                                                 \
  };                                                                                              \
  PBL_DEVICE_INSTANCE(sym##_intc, struct pbl_gpio_sf32lb_intc, struct pbl_intc, intc){            \
    .intc = PBL_INTC_INIT(sym##_intc, _name "_intc", nullptr, &sym##_inst.port.dev, &s_intc_ops), \
    .port = &sym##_inst,                                                                          \
  };                                                                                              \
  PBL_IRQ_CONNECT_NUM(_irq, CONFIG_GPIO_SF32LB_IRQ_PRIO, prv_irq_handler, &sym##_inst, 0)

#ifdef CONFIG_PINCTRL_SF32LB52
#include PBL_DT_INSTANCES(sifli_sf32lb52_gpio)
#endif
#ifdef CONFIG_PINCTRL_SF32LB57
#include PBL_DT_INSTANCES(sifli_sf32lb57_gpio)
#endif
