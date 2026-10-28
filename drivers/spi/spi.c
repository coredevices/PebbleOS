/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/spi.h>
#include <pbl/kernel/sem.h>
#include <pbl/kernel/types.h>
#include <pbl/logging/logging.h>
#include <pbl/util/misc.h>

PBL_LOG_MODULE_DEFINE(driver_spi, CONFIG_DRIVER_SPI_LOG_LEVEL);

PBL_DEVICE_CLASS_DEFINE(pbl_spi_bus_class, "spi");

struct prv_sync {
  struct pbl_sem done;
  int status;
};

int pbl_spi_bus_init(const struct pbl_device *dev) {
  const struct pbl_spi_bus *bus = container_of(dev, const struct pbl_spi_bus, dev);

  *bus->state = (struct pbl_spi_bus_state){0};
  pbl_sem_init(&bus->state->lock, 1, 1);

  return bus->ops->init(bus);
}

// The device mode sets the polarity, not the GPIO flags, as Linux does for cs-gpios
static bool prv_cs_level(const struct pbl_spi_dev *dev, bool active) {
  return active == ((dev->mode & PBL_SPI_CS_HIGH) != 0U);
}

static void prv_cs_set(const struct pbl_spi_dev *dev, bool active) {
  const struct pbl_gpio *cs = &dev->bus->cs_gpios[dev->cs];

  if (pbl_gpio_is_connected(cs)) {
    (void)pbl_gpio_set_raw(cs, prv_cs_level(dev, active));
  }
}

static void prv_end(const struct pbl_spi_bus *bus) {
  prv_cs_set(bus->state->dev, false);
  bus->ops->disable(bus);
  pbl_sem_give(&bus->state->lock);
}

void pbl_spi_bus_complete(const struct pbl_spi_bus *bus, int status) {
  struct pbl_spi_bus_state *state = bus->state;
  pbl_spi_callback_t cb = state->cb;
  void *arg = state->arg;

  if (status == 0 && ++state->idx < state->num_xfers) {
    status = bus->ops->start(bus, &state->xfers[state->idx]);
    if (status == 0) {
      return;
    }
  }

  prv_end(bus);
  cb(status, arg);
}

static int prv_validate(const struct pbl_spi_dev *dev, const struct pbl_spi_xfer *xfers,
                        size_t num_xfers) {
  if (dev->cs >= dev->bus->num_cs || num_xfers == 0U) {
    return -EINVAL;
  }

  for (size_t i = 0U; i < num_xfers; i++) {
    if (xfers[i].len == 0U) {
      return -EINVAL;
    }
  }

  return 0;
}

int pbl_spi_setup(const struct pbl_spi_dev *dev) {
  const struct pbl_spi_bus *bus = dev->bus;
  const struct pbl_gpio *cs;
  int ret;

  if (!pbl_device_is_ready(&bus->dev)) {
    return -ENODEV;
  }

  if (dev->cs >= bus->num_cs) {
    return -EINVAL;
  }

  (void)pbl_sem_take(&bus->state->lock, PBL_FOREVER);
  ret = bus->ops->configure(bus, dev);
  pbl_sem_give(&bus->state->lock);
  if (ret != 0) {
    return ret;
  }

  cs = &bus->cs_gpios[dev->cs];
  if (!pbl_gpio_is_connected(cs)) {
    return 0;
  }

  if (pbl_device_init(&cs->port->dev) != 0) {
    return -ENODEV;
  }

  return pbl_gpio_configure(cs,
                            prv_cs_level(dev, false) ? PBL_GPIO_OUTPUT_HIGH : PBL_GPIO_OUTPUT_LOW);
}

int pbl_spi_transfer_async(const struct pbl_spi_dev *dev, const struct pbl_spi_xfer *xfers,
                           size_t num_xfers, pbl_spi_callback_t cb, void *arg) {
  const struct pbl_spi_bus *bus = dev->bus;
  struct pbl_spi_bus_state *state = bus->state;
  int ret;

  if (!pbl_device_is_ready(&bus->dev)) {
    return -ENODEV;
  }

  ret = prv_validate(dev, xfers, num_xfers);
  if (ret != 0) {
    return ret;
  }

  (void)pbl_sem_take(&state->lock, PBL_FOREVER);

  ret = bus->ops->configure(bus, dev);
  if (ret != 0) {
    pbl_sem_give(&state->lock);
    return ret;
  }

  state->dev = dev;
  state->xfers = xfers;
  state->num_xfers = num_xfers;
  state->idx = 0U;
  state->cb = cb;
  state->arg = arg;

  bus->ops->enable(bus);
  prv_cs_set(dev, true);

  ret = bus->ops->start(bus, &xfers[0]);
  if (ret != 0) {
    prv_end(bus);
  }

  return ret;
}

static void prv_sync_complete(int status, void *arg) {
  struct prv_sync *sync = arg;

  sync->status = status;
  pbl_sem_give(&sync->done);
}

int pbl_spi_transfer(const struct pbl_spi_dev *dev, const struct pbl_spi_xfer *xfers,
                     size_t num_xfers) {
  struct prv_sync sync;
  int ret;

  pbl_sem_init(&sync.done, 0, 1);

  ret = pbl_spi_transfer_async(dev, xfers, num_xfers, prv_sync_complete, &sync);
  if (ret == 0) {
    (void)pbl_sem_take(&sync.done, PBL_FOREVER);
    ret = sync.status;
  }

  pbl_sem_deinit(&sync.done);

  if (ret != 0) {
    PBL_LOG_DBG("Transfer on %s failed (%d)", dev->bus->dev.name, ret);
  }

  return ret;
}
