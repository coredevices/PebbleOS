/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/i2c.h>
#include <pbl/kernel/delay.h>
#include <pbl/kernel/mutex.h>
#include <pbl/kernel/sem.h>
#include <pbl/kernel/types.h>
#include <pbl/logging/logging.h>
#include <pbl/mcu/cache.h>
#include <pbl/services/analytics/analytics.h>
#include <pbl/util/misc.h>

PBL_LOG_MODULE_DEFINE(driver_i2c, CONFIG_DRIVER_I2C_LOG_LEVEL);

#define TRANSFER_TIMEOUT_MS 1000U

PBL_DEVICE_CLASS_DEFINE(pbl_i2c_bus_class, "i2c");

int pbl_i2c_bus_init(const struct pbl_device *dev) {
  const struct pbl_i2c_bus *bus = container_of(dev, const struct pbl_i2c_bus, dev);

  *bus->state = (struct pbl_i2c_bus_state){0};
  pbl_sem_init(&bus->state->done, 0, 1);
  pbl_mutex_init(&bus->state->mutex);

  return bus->ops->init(bus);
}

void pbl_i2c_bus_complete(const struct pbl_i2c_bus *bus, int status) {
  bus->state->status = status;
  pbl_sem_give(&bus->state->done);
}

static int prv_validate(const struct pbl_i2c_msg *msgs, uint8_t num_msgs) {
  if (num_msgs == 0U || (msgs[0].flags & PBL_I2C_MSG_NOSTART) != 0U) {
    return -EINVAL;
  }

  for (uint8_t i = 0U; i < num_msgs; i++) {
    if ((msgs[i].flags & PBL_I2C_MSG_DMA_SAFE) != 0U &&
        ((uintptr_t)msgs[i].buf & (DCACHE_LINE_SIZE_MAX - 1U)) != 0U) {
      return -EINVAL;
    }

    if ((msgs[i].flags & PBL_I2C_MSG_NOSTART) != 0U &&
        ((msgs[i].flags & PBL_I2C_MSG_READ) != 0U ||
         (msgs[i - 1U].flags & PBL_I2C_MSG_READ) != 0U)) {
      return -EINVAL;
    }
  }

  return 0;
}

static int prv_transfer_locked(const struct pbl_i2c_bus *bus, uint16_t addr,
                               struct pbl_i2c_msg *msgs, uint8_t num_msgs) {
  struct pbl_i2c_bus_state *state = bus->state;
  int ret;

  ret = bus->ops->start(bus, addr, msgs, num_msgs);
  if (ret != 0) {
    return ret;
  }

  if (pbl_sem_take(&state->done, PBL_TICKS(pbl_ms_to_ticks(TRANSFER_TIMEOUT_MS))) != 0) {
    bus->ops->abort(bus);
    // The transfer may have completed between the timeout and the abort
    (void)pbl_sem_take(&state->done, PBL_NO_WAIT);
    return -ETIMEDOUT;
  }

  return state->status;
}

int pbl_i2c_transfer(const struct pbl_i2c_dev *dev, struct pbl_i2c_msg *msgs, uint8_t num_msgs) {
  const struct pbl_i2c_bus *bus = dev->bus;
  int ret;

  if (!pbl_device_is_ready(&bus->dev)) {
    return -ENODEV;
  }

  ret = prv_validate(msgs, num_msgs);
  if (ret != 0) {
    return ret;
  }

  pbl_mutex_lock(&bus->state->mutex, PBL_FOREVER);
  bus->ops->enable(bus);
  ret = prv_transfer_locked(bus, dev->addr, msgs, num_msgs);
  bus->ops->disable(bus);
  pbl_mutex_unlock(&bus->state->mutex);

  if (ret != 0) {
    PBL_ANALYTICS_ADD(i2c_transfer_error_count, 1);
    PBL_LOG_DBG("Transfer to 0x%02x on %s failed (%d)", dev->addr, bus->dev.name, ret);
  }

  return ret;
}

int pbl_i2c_reg_update_byte(const struct pbl_i2c_dev *dev, uint8_t reg, uint8_t mask, uint8_t val) {
  uint8_t old;
  uint8_t new;
  int ret;

  ret = pbl_i2c_reg_read_byte(dev, reg, &old);
  if (ret != 0) {
    return ret;
  }

  new = (old & ~mask) | (val & mask);
  if (new == old) {
    return 0;
  }

  return pbl_i2c_reg_write_byte(dev, reg, new);
}
