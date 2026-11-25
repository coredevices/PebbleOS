/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/gpio/stm32.h>
#include <pbl/kernel/compiler.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/mmio.h>
#include <pbl/soc/st/pwr.h>
#include <pbl/util/bits.h>
#include <pbl/util/misc.h>

#include <hardware/stm32_gpio.h>
#include <hardware/stm32h7_exti.h>

// The ports follow each other at 1 KiB steps from port A
#define PORT_INDEX(base) (((base) / STM32_GPIO_PORT_SIZE) % 16U)

// Port holding each EXTI line, NULL when free
static const struct pbl_gpio_stm32 *s_owners[STM32_GPIO_NUM_PINS];
static uintptr_t s_exti;

static const struct pbl_gpio_stm32 *prv_port(const struct pbl_gpio_port *port) {
  return PBL_CONTAINER_OF(port, const struct pbl_gpio_stm32, port);
}

static const struct pbl_gpio_stm32 *prv_intc_port(const struct pbl_intc *intc) {
  return PBL_CONTAINER_OF(intc, const struct pbl_gpio_stm32_intc, intc)->port;
}

static void prv_field(const struct pbl_gpio_stm32 *gpio, uint32_t offset, uint32_t mask,
                      uint32_t value) {
  pbl_modifyl(mask, PBL_FIELD_PREP(mask, value), gpio->base + offset);
}

static int prv_configure(const struct pbl_gpio_port *port, uint8_t pin, uint32_t flags) {
  const struct pbl_gpio_stm32 *gpio = prv_port(port);
  uint32_t pupd = GPIO_PUPDR_NONE;
  int ret;

  if ((flags & (PBL_GPIO_INPUT | PBL_GPIO_OUTPUT)) == 0U) {
    return -EINVAL;
  }

  if ((flags & PBL_GPIO_PULL_UP) != 0U) {
    pupd = GPIO_PUPDR_UP;
  } else if ((flags & PBL_GPIO_PULL_DOWN) != 0U) {
    pupd = GPIO_PUPDR_DOWN;
  }

  ret = pbl_hwspinlock_lock(&gpio->hwlock);
  if (ret != 0) {
    return ret;
  }

  prv_field(gpio, STM32_GPIO_PUPDR_OFFSET, GPIO_PUPDR_MASK(pin), pupd);
  if ((flags & PBL_GPIO_OUTPUT) != 0U) {
    if ((flags & PBL_GPIO_OUTPUT_INIT_HIGH) != 0U) {
      pbl_writel(GPIO_BSRR_SET(pin), gpio->base + STM32_GPIO_BSRR_OFFSET);
    } else if ((flags & PBL_GPIO_OUTPUT_INIT_LOW) != 0U) {
      pbl_writel(GPIO_BSRR_RESET(pin), gpio->base + STM32_GPIO_BSRR_OFFSET);
    }
    prv_field(gpio, STM32_GPIO_OTYPER_OFFSET, PBL_BIT(pin),
              (flags & PBL_GPIO_OPEN_DRAIN) != 0U ? 1U : 0U);
    prv_field(gpio, STM32_GPIO_MODER_OFFSET, GPIO_MODER_MASK(pin), GPIO_MODER_OUTPUT);
  } else {
    prv_field(gpio, STM32_GPIO_MODER_OFFSET, GPIO_MODER_MASK(pin), GPIO_MODER_INPUT);
  }
  pbl_hwspinlock_unlock(&gpio->hwlock);

  return 0;
}

static int prv_get(const struct pbl_gpio_port *port, uint8_t pin) {
  return (pbl_readl(prv_port(port)->base + STM32_GPIO_IDR_OFFSET) & PBL_BIT(pin)) != 0U;
}

static int prv_set(const struct pbl_gpio_port *port, uint8_t pin, bool level) {
  pbl_writel(level ? GPIO_BSRR_SET(pin) : GPIO_BSRR_RESET(pin),
             prv_port(port)->base + STM32_GPIO_BSRR_OFFSET);

  return 0;
}

const struct pbl_gpio_port_ops pbl_gpio_stm32_ops = {
  .configure = prv_configure,
  .get = prv_get,
  .set = prv_set,
};

static bool prv_routed(const struct pbl_gpio_stm32 *gpio, uint32_t hwirq) {
  uint32_t exticr = pbl_readl(gpio->syscfg + STM32H7_SYSCFG_EXTICR_OFFSET(hwirq));

  return PBL_FIELD_GET(SYSCFG_EXTICR_MASK(hwirq), exticr) == PORT_INDEX(gpio->base);
}

// A line serves the port that configures it, unless another port holds it enabled, on this core
// or on the other
static int prv_intc_configure(const struct pbl_intc *intc, uint32_t hwirq, uint32_t type) {
  const struct pbl_gpio_stm32 *gpio = prv_intc_port(intc);
  uint32_t bit = PBL_BIT(hwirq);
  int ret;

  if ((type & (PBL_IRQ_TYPE_LEVEL_HIGH | PBL_IRQ_TYPE_LEVEL_LOW)) != 0U) {
    return -ENOTSUP;
  }

  ret = pbl_hwspinlock_lock(&gpio->exti_hwlock);
  if (ret != 0) {
    return ret;
  }

  if ((s_owners[hwirq] != nullptr && s_owners[hwirq] != gpio) ||
      (!prv_routed(gpio, hwirq) &&
       (pbl_readl(gpio->exti + STM32H7_EXTI_OTHER_IMR1_OFFSET) & bit) != 0U)) {
    ret = -EBUSY;
  } else {
    pbl_modifyl(SYSCFG_EXTICR_MASK(hwirq),
                PBL_FIELD_PREP(SYSCFG_EXTICR_MASK(hwirq), PORT_INDEX(gpio->base)),
                gpio->syscfg + STM32H7_SYSCFG_EXTICR_OFFSET(hwirq));
    pbl_modifyl(bit, (type & PBL_IRQ_TYPE_EDGE_RISING) != 0U ? bit : 0U,
                gpio->exti + STM32H7_EXTI_RTSR1_OFFSET);
    pbl_modifyl(bit, (type & PBL_IRQ_TYPE_EDGE_FALLING) != 0U ? bit : 0U,
                gpio->exti + STM32H7_EXTI_FTSR1_OFFSET);
  }
  pbl_hwspinlock_unlock(&gpio->exti_hwlock);

  return ret;
}

// The other core may have taken the line since it was configured
static int prv_intc_enable(const struct pbl_intc *intc, uint32_t hwirq) {
  const struct pbl_gpio_stm32 *gpio = prv_intc_port(intc);
  int ret;

  ret = pbl_hwspinlock_lock(&gpio->exti_hwlock);
  if (ret != 0) {
    return ret;
  }

  if ((s_owners[hwirq] != nullptr && s_owners[hwirq] != gpio) || !prv_routed(gpio, hwirq)) {
    ret = -EBUSY;
  } else {
    s_owners[hwirq] = gpio;
    pbl_writel(PBL_BIT(hwirq), gpio->exti + STM32H7_EXTI_CORE_PR1_OFFSET);
    pbl_modifyl(0U, PBL_BIT(hwirq), gpio->exti + STM32H7_EXTI_CORE_IMR1_OFFSET);
  }
  pbl_hwspinlock_unlock(&gpio->exti_hwlock);

  return ret;
}

static int prv_intc_disable(const struct pbl_intc *intc, uint32_t hwirq) {
  const struct pbl_gpio_stm32 *gpio = prv_intc_port(intc);

  pbl_irq_lock();
  if (s_owners[hwirq] == gpio) {
    pbl_modifyl(PBL_BIT(hwirq), 0U, gpio->exti + STM32H7_EXTI_CORE_IMR1_OFFSET);
    s_owners[hwirq] = nullptr;
  }
  pbl_irq_unlock();

  return 0;
}

// Only the lines on a wakeup pin wake the SoC from Standby
static int prv_intc_set_wake(const struct pbl_intc *intc, uint32_t hwirq, bool on) {
  return pbl_soc_stm32h7_wakeup_pin_set(prv_intc_port(intc)->base, hwirq, on);
}

const struct pbl_intc_ops pbl_gpio_stm32_intc_ops = {
  .configure = prv_intc_configure,
  .enable = prv_intc_enable,
  .disable = prv_intc_disable,
  .set_wake = prv_intc_set_wake,
};

static void prv_exti_isr(uint32_t lines) {
  uint32_t pending = pbl_readl(s_exti + STM32H7_EXTI_CORE_PR1_OFFSET) &
                     pbl_readl(s_exti + STM32H7_EXTI_CORE_IMR1_OFFSET) & lines;

  pbl_writel(pending, s_exti + STM32H7_EXTI_CORE_PR1_OFFSET);

  while (pending != 0U) {
    uint32_t line = (uint32_t)PBL_CTZ(pending);

    if (s_owners[line] != nullptr) {
      pbl_intc_dispatch(s_owners[line]->port.intc, line);
    }
    pending &= pending - 1U;
  }
}

static void prv_exti0_isr(void) {
  prv_exti_isr(PBL_BIT(0));
}

static void prv_exti1_isr(void) {
  prv_exti_isr(PBL_BIT(1));
}

static void prv_exti2_isr(void) {
  prv_exti_isr(PBL_BIT(2));
}

static void prv_exti3_isr(void) {
  prv_exti_isr(PBL_BIT(3));
}

static void prv_exti4_isr(void) {
  prv_exti_isr(PBL_BIT(4));
}

static void prv_exti9_5_isr(void) {
  prv_exti_isr(PBL_GENMASK(9, 5));
}

static void prv_exti15_10_isr(void) {
  prv_exti_isr(PBL_GENMASK(15, 10));
}

PBL_IRQ_CONNECT(EXTI0, CONFIG_GPIO_STM32_IRQ_PRIO, prv_exti0_isr, , 0);
PBL_IRQ_CONNECT(EXTI1, CONFIG_GPIO_STM32_IRQ_PRIO, prv_exti1_isr, , 0);
PBL_IRQ_CONNECT(EXTI2, CONFIG_GPIO_STM32_IRQ_PRIO, prv_exti2_isr, , 0);
PBL_IRQ_CONNECT(EXTI3, CONFIG_GPIO_STM32_IRQ_PRIO, prv_exti3_isr, , 0);
PBL_IRQ_CONNECT(EXTI4, CONFIG_GPIO_STM32_IRQ_PRIO, prv_exti4_isr, , 0);
PBL_IRQ_CONNECT(EXTI9_5, CONFIG_GPIO_STM32_IRQ_PRIO, prv_exti9_5_isr, , 0);
PBL_IRQ_CONNECT(EXTI15_10, CONFIG_GPIO_STM32_IRQ_PRIO, prv_exti15_10_isr, , 0);

int pbl_gpio_stm32_init(const struct pbl_device *dev) {
  const struct pbl_gpio_stm32 *gpio = PBL_CONTAINER_OF(dev, const struct pbl_gpio_stm32, port.dev);
  int ret;

  ret = pbl_clk_enable(&gpio->clk);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_clk_enable(&gpio->syscfg_clk);
  if (ret != 0) {
    return ret;
  }

  pbl_irq_lock();
  if (s_exti == 0U) {
    s_exti = gpio->exti;
    pbl_irq_enable(PBL_IRQN(EXTI0));
    pbl_irq_enable(PBL_IRQN(EXTI1));
    pbl_irq_enable(PBL_IRQN(EXTI2));
    pbl_irq_enable(PBL_IRQN(EXTI3));
    pbl_irq_enable(PBL_IRQN(EXTI4));
    pbl_irq_enable(PBL_IRQN(EXTI9_5));
    pbl_irq_enable(PBL_IRQN(EXTI15_10));
  }
  pbl_irq_unlock();

  return 0;
}

#include PBL_DT_INSTANCES(st_stm32h743_pinctrl_gpio)
