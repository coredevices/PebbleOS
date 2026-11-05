/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <string.h>

#include <pbl/drivers/gpio/nrf5.h>
#include <pbl/drivers/i2c/nrf5.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/mmio.h>
#include <pbl/util/misc.h>

#include <hardware/nrf52_twim.h>

// EasyDMA only reaches data RAM
#define IS_IN_RAM(p) (((uintptr_t)(p) & 0xe0000000U) == 0x20000000U)

static const struct {
  uintptr_t base;
  pbl_irq_t irqn;
} s_instances[] = {
  {NRF52_TWIM0_BASE, PBL_IRQN(SPI0_SPIM0_SPIS0_TWI0_TWIM0_TWIS0)},
  {NRF52_TWIM1_BASE, PBL_IRQN(SPI1_SPIM1_SPIS1_TWI1_TWIM1_TWIS1)},
};

static const struct pbl_i2c_nrf5 *prv_i2c(const struct pbl_i2c_bus *bus) {
  return container_of(bus, const struct pbl_i2c_nrf5, bus);
}

static inline uint32_t prv_getreg(const struct pbl_i2c_nrf5 *i2c, uint32_t offset) {
  return pbl_readl(s_instances[i2c->index].base + offset);
}

static inline void prv_putreg(const struct pbl_i2c_nrf5 *i2c, uint32_t offset, uint32_t value) {
  pbl_writel(value, s_instances[i2c->index].base + offset);
}

static void prv_event_clear(const struct pbl_i2c_nrf5 *i2c, uint32_t offset) {
  prv_putreg(i2c, offset, 0U);
  (void)prv_getreg(i2c, offset);
}

static bool prv_event_take(const struct pbl_i2c_nrf5 *i2c, uint32_t offset) {
  if (prv_getreg(i2c, offset) == 0U) {
    return false;
  }

  prv_event_clear(i2c, offset);

  return true;
}

static int prv_frequency(uint32_t clock_hz, uint32_t *frequency) {
  switch (clock_hz) {
    case 100000U:
      *frequency = TWIM_FREQUENCY_K100;
      return 0;
    case 250000U:
      *frequency = TWIM_FREQUENCY_K250;
      return 0;
    case 400000U:
      *frequency = TWIM_FREQUENCY_K400;
      return 0;
    default:
      return -EINVAL;
  }
}

static int prv_init(const struct pbl_i2c_bus *bus) {
  const struct pbl_i2c_nrf5 *i2c = prv_i2c(bus);
  int ret;

  if (i2c->index >= ARRAY_LENGTH(s_instances)) {
    return -EINVAL;
  }

  ret = prv_frequency(i2c->clock_hz, &i2c->state->frequency);
  if (ret != 0) {
    return ret;
  }

  if (pbl_device_init(&i2c->scl.port->dev) != 0 || pbl_device_init(&i2c->sda.port->dev) != 0) {
    return -ENODEV;
  }

  // Keep the bus lines defined while the TWIM is disabled
  ret = pbl_gpio_configure(&i2c->scl, PBL_GPIO_INPUT | PBL_GPIO_OPEN_DRAIN);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_gpio_configure(&i2c->sda, PBL_GPIO_INPUT | PBL_GPIO_OPEN_DRAIN);
  if (ret != 0) {
    return ret;
  }

  pbl_irq_enable(s_instances[i2c->index].irqn);

  return 0;
}

static void prv_enable(const struct pbl_i2c_bus *bus) {
  const struct pbl_i2c_nrf5 *i2c = prv_i2c(bus);

  prv_putreg(i2c, NRF52_TWIM_PSEL_SCL_OFFSET, pbl_gpio_nrf5_pin(&i2c->scl));
  prv_putreg(i2c, NRF52_TWIM_PSEL_SDA_OFFSET, pbl_gpio_nrf5_pin(&i2c->sda));
  prv_putreg(i2c, NRF52_TWIM_FREQUENCY_OFFSET, i2c->state->frequency);
  prv_putreg(i2c, NRF52_TWIM_ENABLE_OFFSET, TWIM_ENABLE_ENABLED);
}

static void prv_disable(const struct pbl_i2c_bus *bus) {
  const struct pbl_i2c_nrf5 *i2c = prv_i2c(bus);

  prv_putreg(i2c, NRF52_TWIM_INTENCLR_OFFSET, TWIM_INT_STOPPED | TWIM_INT_ERROR);
  prv_putreg(i2c, NRF52_TWIM_SHORTS_OFFSET, 0U);
  prv_putreg(i2c, NRF52_TWIM_ENABLE_OFFSET, TWIM_ENABLE_DISABLED);
  prv_event_clear(i2c, NRF52_TWIM_EVENTS_STOPPED_OFFSET);
  prv_event_clear(i2c, NRF52_TWIM_EVENTS_ERROR_OFFSET);
  pbl_irq_clear_pending(s_instances[i2c->index].irqn);
}

static void prv_abort(const struct pbl_i2c_bus *bus) {
  const struct pbl_i2c_nrf5 *i2c = prv_i2c(bus);

  prv_disable(bus);
  prv_putreg(i2c, NRF52_TWIM_ENABLE_OFFSET, TWIM_ENABLE_ENABLED);
}

// The TWIM issues one write, one read, or a write then a read after a repeated START. Writes
// continued with PBL_I2C_MSG_NOSTART are gathered in the bounce buffer.
static int prv_prepare(struct pbl_i2c_nrf5_state *state, struct pbl_i2c_msg *msgs, uint8_t num_msgs,
                       const uint8_t **tx_buf, uint8_t **rx_buf) {
  uint8_t i = 0U;

  state->tx = false;
  state->rx = false;
  state->tx_len = 0U;
  state->rx_len = 0U;

  if ((msgs[0].flags & PBL_I2C_MSG_READ) == 0U) {
    state->tx = true;
    *tx_buf = msgs[0].buf;
    state->tx_len = msgs[0].len;

    for (i = 1U; i < num_msgs && (msgs[i].flags & PBL_I2C_MSG_NOSTART) != 0U; i++) {
      if (*tx_buf != state->bounce) {
        if (state->tx_len > sizeof(state->bounce)) {
          return -ENOTSUP;
        }
        memcpy(state->bounce, *tx_buf, state->tx_len);
        *tx_buf = state->bounce;
      }

      if (state->tx_len + msgs[i].len > sizeof(state->bounce)) {
        return -ENOTSUP;
      }
      memcpy(&state->bounce[state->tx_len], msgs[i].buf, msgs[i].len);
      state->tx_len += msgs[i].len;
    }
  }

  if (i < num_msgs) {
    if ((msgs[i].flags & PBL_I2C_MSG_READ) == 0U || i + 1U != num_msgs) {
      return -ENOTSUP;
    }

    state->rx = true;
    *rx_buf = msgs[i].buf;
    state->rx_len = msgs[i].len;
  }

  if ((state->tx_len > 0U && (!IS_IN_RAM(*tx_buf) || state->tx_len > TWIM_MAXCNT_MASK)) ||
      (state->rx_len > 0U && (!IS_IN_RAM(*rx_buf) || state->rx_len > TWIM_MAXCNT_MASK))) {
    return -EINVAL;
  }

  return 0;
}

static int prv_start(const struct pbl_i2c_bus *bus, uint16_t addr, struct pbl_i2c_msg *msgs,
                     uint8_t num_msgs) {
  const struct pbl_i2c_nrf5 *i2c = prv_i2c(bus);
  struct pbl_i2c_nrf5_state *state = i2c->state;
  const uint8_t *tx_buf = nullptr;
  uint8_t *rx_buf = nullptr;
  uint32_t shorts;
  int ret;

  ret = prv_prepare(state, msgs, num_msgs, &tx_buf, &rx_buf);
  if (ret != 0) {
    return ret;
  }

  state->error = 0;

  prv_putreg(i2c, NRF52_TWIM_ADDRESS_OFFSET, addr);
  prv_event_clear(i2c, NRF52_TWIM_EVENTS_STOPPED_OFFSET);
  prv_event_clear(i2c, NRF52_TWIM_EVENTS_ERROR_OFFSET);
  prv_event_clear(i2c, NRF52_TWIM_EVENTS_LASTTX_OFFSET);
  prv_event_clear(i2c, NRF52_TWIM_EVENTS_LASTRX_OFFSET);

  if (state->tx) {
    prv_putreg(i2c, NRF52_TWIM_TXD_PTR_OFFSET, (uintptr_t)tx_buf);
    prv_putreg(i2c, NRF52_TWIM_TXD_MAXCNT_OFFSET, state->tx_len);
  }

  if (state->rx) {
    prv_putreg(i2c, NRF52_TWIM_RXD_PTR_OFFSET, (uintptr_t)rx_buf);
    prv_putreg(i2c, NRF52_TWIM_RXD_MAXCNT_OFFSET, state->rx_len);
  }

  if (state->tx && state->rx) {
    shorts = TWIM_SHORTS_LASTTX_STARTRX | TWIM_SHORTS_LASTRX_STOP;
  } else if (state->tx) {
    shorts = TWIM_SHORTS_LASTTX_STOP;
  } else {
    shorts = TWIM_SHORTS_LASTRX_STOP;
  }

  prv_putreg(i2c, NRF52_TWIM_SHORTS_OFFSET, shorts);
  prv_putreg(i2c, NRF52_TWIM_INTENSET_OFFSET, TWIM_INT_STOPPED | TWIM_INT_ERROR);
  prv_putreg(i2c, state->tx ? NRF52_TWIM_TASKS_STARTTX_OFFSET : NRF52_TWIM_TASKS_STARTRX_OFFSET,
             1U);

  // An empty transfer has no last byte to stop on
  if ((state->tx ? state->tx_len : state->rx_len) == 0U) {
    prv_putreg(i2c, NRF52_TWIM_TASKS_STOP_OFFSET, 1U);
  }

  return 0;
}

void pbl_i2c_nrf5_irq_handler(const struct pbl_i2c_nrf5 *i2c) {
  struct pbl_i2c_nrf5_state *state = i2c->state;
  bool stopped = prv_event_take(i2c, NRF52_TWIM_EVENTS_STOPPED_OFFSET);
  uint32_t errorsrc;

  if (prv_event_take(i2c, NRF52_TWIM_EVENTS_ERROR_OFFSET) && !stopped) {
    bool lasttx = prv_event_take(i2c, NRF52_TWIM_EVENTS_LASTTX_OFFSET);

    state->error = -EIO;
    prv_putreg(i2c, NRF52_TWIM_INTENCLR_OFFSET, TWIM_INT_ERROR);

    // Stop by hand, unless the LASTTX shortcut already does
    if (!lasttx || (prv_getreg(i2c, NRF52_TWIM_SHORTS_OFFSET) & TWIM_SHORTS_LASTTX_STOP) == 0U) {
      prv_putreg(i2c, NRF52_TWIM_TASKS_RESUME_OFFSET, 1U);
      prv_putreg(i2c, NRF52_TWIM_TASKS_STOP_OFFSET, 1U);
    }

    return;
  }

  if (!stopped) {
    return;
  }

  prv_putreg(i2c, NRF52_TWIM_INTENCLR_OFFSET, TWIM_INT_STOPPED | TWIM_INT_ERROR);
  prv_putreg(i2c, NRF52_TWIM_SHORTS_OFFSET, 0U);
  prv_event_clear(i2c, NRF52_TWIM_EVENTS_LASTTX_OFFSET);
  prv_event_clear(i2c, NRF52_TWIM_EVENTS_LASTRX_OFFSET);

  errorsrc = prv_getreg(i2c, NRF52_TWIM_ERRORSRC_OFFSET);
  prv_putreg(i2c, NRF52_TWIM_ERRORSRC_OFFSET, errorsrc);

  if ((errorsrc & TWIM_ERRORSRC_ANACK) != 0U) {
    state->error = -ENXIO;
  } else if (errorsrc != 0U) {
    state->error = -EIO;
  } else if (state->error == 0 &&
             ((state->tx && prv_getreg(i2c, NRF52_TWIM_TXD_AMOUNT_OFFSET) != state->tx_len) ||
              (state->rx && prv_getreg(i2c, NRF52_TWIM_RXD_AMOUNT_OFFSET) != state->rx_len))) {
    // A short transfer without an error is a premature STOP on the bus: reset the state machine
    state->error = -EIO;
    prv_putreg(i2c, NRF52_TWIM_ENABLE_OFFSET, TWIM_ENABLE_DISABLED);
    prv_putreg(i2c, NRF52_TWIM_ENABLE_OFFSET, TWIM_ENABLE_ENABLED);
  }

  pbl_i2c_bus_complete(&i2c->bus, state->error);
}

const struct pbl_i2c_bus_ops pbl_i2c_nrf5_ops = {
  .init = prv_init,
  .enable = prv_enable,
  .disable = prv_disable,
  .start = prv_start,
  .abort = prv_abort,
};
