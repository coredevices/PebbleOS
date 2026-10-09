/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <string.h>

#include <pbl/drivers/gpio/nrf5.h>
#include <pbl/drivers/i2c/nrf5.h>
#include <pbl/util/misc.h>

#include <nrfx.h>
#include <system/passert.h>

static const struct pbl_i2c_nrf5 *prv_i2c(const struct pbl_i2c_bus *bus) {
  return container_of(bus, const struct pbl_i2c_nrf5, bus);
}

static void prv_twim_evt_handler(nrfx_twim_evt_t const *evt, void *ctx) {
  const struct pbl_i2c_bus *bus = ctx;

  pbl_i2c_bus_event(
      bus, (evt->type == NRFX_TWIM_EVT_DONE) ? PBL_I2C_EVENT_COMPLETE : PBL_I2C_EVENT_ERROR);
}

static int prv_twim_init(const struct pbl_i2c_bus *bus) {
  const struct pbl_i2c_nrf5 *i2c = prv_i2c(bus);
  nrfx_twim_config_t config =
      NRFX_TWIM_DEFAULT_CONFIG(pbl_gpio_nrf5_pin(&i2c->scl), pbl_gpio_nrf5_pin(&i2c->sda));

  config.frequency = i2c->frequency;
  config.hold_bus_uninit = true;

  if (nrfx_twim_init(&i2c->twim, &config, prv_twim_evt_handler, (void *)bus) != NRFX_SUCCESS) {
    return -EIO;
  }

  return 0;
}

static int prv_init(const struct pbl_i2c_bus *bus) {
  const struct pbl_i2c_nrf5 *i2c = prv_i2c(bus);

  if (pbl_device_init(&i2c->scl.port->dev) != 0 || pbl_device_init(&i2c->sda.port->dev) != 0) {
    return -ENODEV;
  }

  int ret = prv_twim_init(bus);

  if (ret == 0) {
    nrfx_twim_uninit(&i2c->twim);
  }

  return ret;
}

static void prv_enable(const struct pbl_i2c_bus *bus) {
  PBL_ASSERTN(prv_twim_init(bus) == 0);
  nrfx_twim_enable(&prv_i2c(bus)->twim);
}

static void prv_disable(const struct pbl_i2c_bus *bus) {
  const struct pbl_i2c_nrf5 *i2c = prv_i2c(bus);

  nrfx_twim_disable(&i2c->twim);
  nrfx_twim_uninit(&i2c->twim);
}

static bool prv_is_busy(const struct pbl_i2c_bus *bus) {
  return nrfx_twim_is_busy(&prv_i2c(bus)->twim);
}

static void prv_abort_transfer(const struct pbl_i2c_bus *bus) {
  const struct pbl_i2c_nrf5 *i2c = prv_i2c(bus);

  nrfx_twim_disable(&i2c->twim);
  nrfx_twim_enable(&i2c->twim);
}

static void prv_start_transfer(const struct pbl_i2c_bus *bus) {
  const struct pbl_i2c_nrf5 *i2c = prv_i2c(bus);
  struct pbl_i2c_transfer *transfer = &bus->state->transfer;
  nrfx_twim_xfer_desc_t desc = {
    .address = transfer->addr,
  };

  if (transfer->with_reg) {
    if (transfer->dir == PBL_I2C_READ) {
      desc.type = NRFX_TWIM_XFER_TXRX;
      desc.primary_length = 1;
      desc.p_primary_buf = &transfer->reg;
      desc.secondary_length = transfer->size;
      desc.p_secondary_buf = transfer->data;
    } else {
      // Register write: send the address and data as one contiguous transfer.
      // The two-buffer write (TXTX) sets up its data phase mid-transfer and can
      // silently drop it depending on the calling context, so never use it.
      uint8_t *wbuf = i2c->state->reg_write_buf;

      if (transfer->size + 1U > sizeof(i2c->state->reg_write_buf)) {
        pbl_i2c_bus_event(bus, PBL_I2C_EVENT_ERROR);
        return;
      }

      wbuf[0] = transfer->reg;
      memcpy(&wbuf[1], transfer->data, transfer->size);
      desc.type = NRFX_TWIM_XFER_TX;
      desc.primary_length = transfer->size + 1U;
      desc.p_primary_buf = wbuf;
    }
  } else {
    desc.type = (transfer->dir == PBL_I2C_READ) ? NRFX_TWIM_XFER_RX : NRFX_TWIM_XFER_TX;
    desc.primary_length = transfer->size;
    desc.p_primary_buf = transfer->data;
  }

  nrfx_err_t rv = nrfx_twim_xfer(&i2c->twim, &desc, 0);
  PBL_ASSERTN(rv == NRFX_SUCCESS);
}

const struct pbl_i2c_bus_ops pbl_i2c_nrf5_ops = {
  .init = prv_init,
  .enable = prv_enable,
  .disable = prv_disable,
  .is_busy = prv_is_busy,
  .start_transfer = prv_start_transfer,
  .abort_transfer = prv_abort_transfer,
};
