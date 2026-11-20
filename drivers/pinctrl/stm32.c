/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/pinctrl/stm32.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/mmio.h>
#include <pbl/util/bits.h>
#include <pbl/util/misc.h>
#include <pbl/util/size.h>

#include <hardware/stm32_gpio.h>

// Modes of a pinmux cell
#define MODE_GPIO   0x00U
#define MODE_AF0    0x01U
#define MODE_ANALOG 0x11U

static const struct pbl_pinctrl_stm32 *prv_ctrl(const struct pbl_pinctrl_controller *ctrl) {
  return PBL_CONTAINER_OF(ctrl, const struct pbl_pinctrl_stm32, ctrl);
}

static void prv_field(uintptr_t reg, uint32_t mask, uint32_t value) {
  pbl_modifyl(mask, PBL_FIELD_PREP(mask, value), reg);
}

static int prv_configure_pin(const struct pbl_pinctrl_stm32 *ctrl, uint32_t pin) {
  uint32_t mode = PBL_FIELD_GET(PBL_PINCTRL_STM32_MODE_MSK, pin);
  uint32_t port = PBL_FIELD_GET(PBL_PINCTRL_STM32_PORT_MSK, pin);
  uint32_t line = PBL_FIELD_GET(PBL_PINCTRL_STM32_LINE_MSK, pin);
  uint32_t moder;
  uintptr_t base;
  int ret;

  if (port >= PBL_PINCTRL_STM32_NUM_PORTS || mode > MODE_ANALOG) {
    return -EINVAL;
  }

  if (mode == MODE_ANALOG) {
    moder = GPIO_MODER_ANALOG;
  } else if (mode >= MODE_AF0) {
    moder = GPIO_MODER_AF;
  } else {
    moder = GPIO_MODER_INPUT;
  }

  ret = pbl_clk_enable(&ctrl->clks[port]);
  if (ret != 0) {
    return ret;
  }

  base = ctrl->base + port * STM32_GPIO_PORT_SIZE;

  pbl_irq_lock();
  if (moder == GPIO_MODER_AF) {
    prv_field(base + STM32_GPIO_AFR_OFFSET(line), GPIO_AFR_MASK(line), mode - MODE_AF0);
  }
  prv_field(base + STM32_GPIO_OTYPER_OFFSET, PBL_BIT(line),
            (pin & PBL_PINCTRL_STM32_OD) != 0U ? 1U : 0U);
  prv_field(base + STM32_GPIO_OSPEEDR_OFFSET, GPIO_OSPEEDR_MASK(line),
            PBL_FIELD_GET(PBL_PINCTRL_STM32_SPEED_MSK, pin));
  prv_field(base + STM32_GPIO_PUPDR_OFFSET, GPIO_PUPDR_MASK(line),
            PBL_FIELD_GET(PBL_PINCTRL_STM32_PUPD_MSK, pin));
  prv_field(base + STM32_GPIO_MODER_OFFSET, GPIO_MODER_MASK(line), moder);
  pbl_irq_unlock();

  // The clock stays on: the port keeps the pin configured
  return 0;
}

static int prv_configure(const struct pbl_pinctrl_controller *ctrl, const uint32_t *pins,
                         uint8_t num_pins, uintptr_t reg) {
  for (uint8_t i = 0U; i < num_pins; i++) {
    int ret = prv_configure_pin(prv_ctrl(ctrl), pins[i]);

    if (ret != 0) {
      return ret;
    }
  }

  return 0;
}

// Pads are numbered 16 per port, from PA0
static int prv_get(const struct pbl_pinctrl_controller *ctrl, uint16_t pad, uint32_t *val) {
  const struct pbl_pinctrl_stm32 *stm32 = prv_ctrl(ctrl);
  uint32_t port = pad / STM32_GPIO_NUM_PINS;
  uint32_t line = pad % STM32_GPIO_NUM_PINS;
  uintptr_t base;

  if (port >= PBL_PINCTRL_STM32_NUM_PORTS) {
    return -EINVAL;
  }

  if (!pbl_clk_is_enabled(&stm32->clks[port])) {
    return -ENODEV;
  }

  base = stm32->base + port * STM32_GPIO_PORT_SIZE;
  *val = PBL_FIELD_GET(GPIO_MODER_MASK(line), pbl_readl(base + STM32_GPIO_MODER_OFFSET)) |
         (PBL_FIELD_GET(GPIO_PUPDR_MASK(line), pbl_readl(base + STM32_GPIO_PUPDR_OFFSET)) << 2) |
         (PBL_FIELD_GET(GPIO_AFR_MASK(line), pbl_readl(base + STM32_GPIO_AFR_OFFSET(line))) << 4);

  return 0;
}

const struct pbl_pinctrl_controller_ops pbl_pinctrl_stm32_ops = {
  .configure = prv_configure,
  .get = prv_get,
};

#define PBL_PINCTRL_STM32_DT_STATE(sym, ...)          \
  static const uint32_t sym##_pins[] = {__VA_ARGS__}; \
  const struct pbl_pinctrl_state sym = {              \
    .ctrl = STM32_PINCTRL,                            \
    .pins = sym##_pins,                               \
    .num_pins = PBL_ARRAY_SIZE(sym##_pins),           \
  }

#include PBL_DT_INSTANCES(st_stm32h743_pinctrl_pinctrl_controller)
#include PBL_DT_INSTANCES(st_stm32h743_pinctrl_pinctrl)
