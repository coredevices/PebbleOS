/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/gpio/tca6408.h>
#include <pbl/util/misc.h>

#define TCA6408_INPUT    0x00U
#define TCA6408_OUTPUT   0x01U
#define TCA6408_POLARITY 0x02U
#define TCA6408_CONFIG   0x03U

static const struct pbl_tca6408 *prv_get(const struct pbl_gpio_port *port) {
  return PBL_CONTAINER_OF(port, const struct pbl_tca6408, port);
}

static uint8_t prv_update(uint8_t reg, uint8_t pin, bool set) {
  return set ? (reg | (1U << pin)) : (reg & ~(1U << pin));
}

static int prv_configure_locked(const struct pbl_tca6408 *dev, uint8_t pin, uint32_t flags) {
  struct pbl_tca6408_state *state = dev->state;
  uint8_t output = state->output;
  uint8_t config;
  int ret;

  if (flags & PBL_GPIO_OUTPUT) {
    if (flags & PBL_GPIO_OUTPUT_INIT_HIGH) {
      output = prv_update(output, pin, true);
    } else if (flags & PBL_GPIO_OUTPUT_INIT_LOW) {
      output = prv_update(output, pin, false);
    }

    // Latch the level before the pin starts driving it
    if (output != state->output) {
      ret = pbl_i2c_reg_write_byte(&dev->i2c, TCA6408_OUTPUT, output);
      if (ret != 0) {
        return ret;
      }
      state->output = output;
    }

    config = prv_update(state->config, pin, false);
  } else if (flags & PBL_GPIO_INPUT) {
    config = prv_update(state->config, pin, true);
  } else {
    return -EINVAL;
  }

  if (config == state->config) {
    return 0;
  }

  ret = pbl_i2c_reg_write_byte(&dev->i2c, TCA6408_CONFIG, config);
  if (ret != 0) {
    return ret;
  }
  state->config = config;

  return 0;
}

static int prv_configure(const struct pbl_gpio_port *port, uint8_t pin, uint32_t flags) {
  const struct pbl_tca6408 *dev = prv_get(port);
  int ret;

  if (pin >= TCA6408_NUM_PINS) {
    return -EINVAL;
  }

  if (flags & (PBL_GPIO_PULL_UP | PBL_GPIO_PULL_DOWN | PBL_GPIO_OPEN_DRAIN)) {
    return -ENOTSUP;
  }

  pbl_mutex_lock(&dev->state->lock, PBL_FOREVER);
  ret = prv_configure_locked(dev, pin, flags);
  pbl_mutex_unlock(&dev->state->lock);

  return ret;
}

static int prv_get_level(const struct pbl_gpio_port *port, uint8_t pin) {
  uint8_t input;
  int ret;

  if (pin >= TCA6408_NUM_PINS) {
    return -EINVAL;
  }

  ret = pbl_i2c_reg_read_byte(&prv_get(port)->i2c, TCA6408_INPUT, &input);
  if (ret != 0) {
    return ret;
  }

  return (input >> pin) & 1U;
}

static int prv_set(const struct pbl_gpio_port *port, uint8_t pin, bool level) {
  const struct pbl_tca6408 *dev = prv_get(port);
  struct pbl_tca6408_state *state = dev->state;
  uint8_t output;
  int ret = 0;

  if (pin >= TCA6408_NUM_PINS) {
    return -EINVAL;
  }

  pbl_mutex_lock(&state->lock, PBL_FOREVER);
  output = prv_update(state->output, pin, level);
  if (output != state->output) {
    ret = pbl_i2c_reg_write_byte(&dev->i2c, TCA6408_OUTPUT, output);
    if (ret == 0) {
      state->output = output;
    }
  }
  pbl_mutex_unlock(&state->lock);

  return ret;
}

int pbl_tca6408_init(const struct pbl_device *dev) {
  const struct pbl_tca6408 *tca = PBL_CONTAINER_OF(dev, const struct pbl_tca6408, port.dev);
  struct pbl_tca6408_state *state = tca->state;
  int ret;

  pbl_mutex_init(&state->lock);

  // The expander keeps its registers across an SoC reset
  ret = pbl_i2c_reg_write_byte(&tca->i2c, TCA6408_POLARITY, 0U);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_i2c_reg_read_byte(&tca->i2c, TCA6408_OUTPUT, &state->output);
  if (ret != 0) {
    return ret;
  }

  return pbl_i2c_reg_read_byte(&tca->i2c, TCA6408_CONFIG, &state->config);
}

const struct pbl_gpio_port_ops pbl_tca6408_ops = {
  .configure = prv_configure,
  .get = prv_get_level,
  .set = prv_set,
};

#include PBL_DT_INSTANCES(ti_tca6408)
