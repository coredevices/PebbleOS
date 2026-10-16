/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/gpio/npm1300.h>
#include <pbl/util/misc.h>

// One register per pin at the block base + register offset + pin
#define NPM1300_GPIO_BASE             0x0600U
#define NPM1300_GPIO_MODE             0x00U
#define NPM1300_GPIO_PULLUP           0x0AU
#define NPM1300_GPIO_PULLDOWN         0x0FU
#define NPM1300_GPIO_OPENDRAIN        0x14U
#define NPM1300_GPIO_STATUS           0x1EU
#define NPM1300_GPIO_MODE_INPUT       0U
#define NPM1300_GPIO_MODE_OUTPUT_HIGH 8U
#define NPM1300_GPIO_MODE_OUTPUT_LOW  9U

static const struct pbl_npm1300_gpio *prv_gpio(const struct pbl_gpio_port *port) {
  return container_of(port, const struct pbl_npm1300_gpio, port);
}

static const struct pbl_npm1300 *prv_pmic(const struct pbl_gpio_port *port) {
  return pbl_npm1300_from_cell(&port->dev);
}

static int prv_read(const struct pbl_npm1300 *pmic, uint16_t offset, uint8_t pin, uint8_t *val) {
  return pbl_npm1300_read(pmic, NPM1300_GPIO_BASE + offset + pin, val);
}

static int prv_write(const struct pbl_npm1300 *pmic, uint16_t offset, uint8_t pin, uint8_t val) {
  return pbl_npm1300_write(pmic, NPM1300_GPIO_BASE + offset + pin, val);
}

static int prv_mode(const struct pbl_npm1300 *pmic, uint8_t pin, uint32_t flags, uint8_t *mode) {
  int ret;

  if (flags & PBL_GPIO_OUTPUT) {
    if (flags & PBL_GPIO_OUTPUT_INIT_HIGH) {
      *mode = NPM1300_GPIO_MODE_OUTPUT_HIGH;
      return 0;
    }

    if (flags & PBL_GPIO_OUTPUT_INIT_LOW) {
      *mode = NPM1300_GPIO_MODE_OUTPUT_LOW;
      return 0;
    }

    // Keep the level of a pin that already is an output
    ret = prv_read(pmic, NPM1300_GPIO_MODE, pin, mode);
    if (ret != 0) {
      return ret;
    }

    if (*mode != NPM1300_GPIO_MODE_OUTPUT_HIGH) {
      *mode = NPM1300_GPIO_MODE_OUTPUT_LOW;
    }

    return 0;
  }

  if (flags & PBL_GPIO_INPUT) {
    *mode = NPM1300_GPIO_MODE_INPUT;
    return 0;
  }

  return -EINVAL;
}

static int prv_configure_locked(const struct pbl_gpio_port *port, uint8_t pin, uint32_t flags) {
  const struct pbl_npm1300 *pmic = prv_pmic(port);
  struct pbl_npm1300_gpio_state *state = prv_gpio(port)->state;
  bool pullup = (flags & PBL_GPIO_PULL_UP) != 0U;
  uint8_t mode;
  int ret;

  ret = prv_mode(pmic, pin, flags, &mode);
  if (ret != 0) {
    return ret;
  }

  if (pullup) {
    state->pullup_mask |= (1U << pin);
  } else {
    state->pullup_mask &= ~(1U << pin);
  }

  // The pull-up of an output follows its level, see prv_set()
  ret = prv_write(pmic, NPM1300_GPIO_PULLUP, pin, pullup && (mode != NPM1300_GPIO_MODE_OUTPUT_LOW));
  if (ret != 0) {
    return ret;
  }

  ret = prv_write(pmic, NPM1300_GPIO_PULLDOWN, pin, (flags & PBL_GPIO_PULL_DOWN) != 0U);
  if (ret != 0) {
    return ret;
  }

  ret = prv_write(pmic, NPM1300_GPIO_OPENDRAIN, pin, (flags & PBL_GPIO_OPEN_DRAIN) != 0U);
  if (ret != 0) {
    return ret;
  }

  return prv_write(pmic, NPM1300_GPIO_MODE, pin, mode);
}

static int prv_configure(const struct pbl_gpio_port *port, uint8_t pin, uint32_t flags) {
  const struct pbl_npm1300 *pmic = prv_pmic(port);
  int ret;

  if (pin >= NPM1300_GPIO_NUM_PINS) {
    return -EINVAL;
  }

  pbl_npm1300_lock(pmic);
  ret = prv_configure_locked(port, pin, flags);
  pbl_npm1300_unlock(pmic);

  return ret;
}

static int prv_get(const struct pbl_gpio_port *port, uint8_t pin) {
  uint8_t status;
  int ret;

  if (pin >= NPM1300_GPIO_NUM_PINS) {
    return -EINVAL;
  }

  ret = pbl_npm1300_read(prv_pmic(port), NPM1300_GPIO_BASE + NPM1300_GPIO_STATUS, &status);
  if (ret != 0) {
    return ret;
  }

  return (status >> pin) & 1U;
}

static int prv_set_locked(const struct pbl_gpio_port *port, uint8_t pin, bool level) {
  const struct pbl_npm1300 *pmic = prv_pmic(port);
  struct pbl_npm1300_gpio_state *state = prv_gpio(port)->state;
  int ret;

  ret = prv_write(pmic, NPM1300_GPIO_MODE, pin,
                  level ? NPM1300_GPIO_MODE_OUTPUT_HIGH : NPM1300_GPIO_MODE_OUTPUT_LOW);
  if (ret != 0) {
    return ret;
  }

  // A pull-up only helps while driving high: drop it while low so no current flows through it
  if ((state->pullup_mask & (1U << pin)) == 0U) {
    return 0;
  }

  return prv_write(pmic, NPM1300_GPIO_PULLUP, pin, level);
}

static int prv_set(const struct pbl_gpio_port *port, uint8_t pin, bool level) {
  const struct pbl_npm1300 *pmic = prv_pmic(port);
  int ret;

  if (pin >= NPM1300_GPIO_NUM_PINS) {
    return -EINVAL;
  }

  pbl_npm1300_lock(pmic);
  ret = prv_set_locked(port, pin, level);
  pbl_npm1300_unlock(pmic);

  return ret;
}

const struct pbl_gpio_port_ops pbl_npm1300_gpio_ops = {
  .configure = prv_configure,
  .get = prv_get,
  .set = prv_set,
};
