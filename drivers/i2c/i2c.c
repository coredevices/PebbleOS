/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/analytics/analytics.h>
#include <pbl/drivers/i2c.h>
#include <pbl/kernel/cache.h>
#include <pbl/kernel/delay.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/mutex.h>
#include <pbl/kernel/sem.h>
#include <pbl/kernel/timer.h>
#include <pbl/kernel/types.h>
#include <pbl/logging/logging.h>
#include <pbl/util/misc.h>

PBL_LOG_MODULE_DEFINE(driver_i2c, CONFIG_DRIVER_I2C_LOG_LEVEL);

#define TRANSFER_TIMEOUT_MS 1000U

PBL_DEVICE_CLASS_DEFINE(pbl_i2c_bus_class, "i2c");

static void prv_kick(const struct pbl_i2c_bus *bus) {
  struct pbl_i2c_bus_state *state = bus->state;
  struct pbl_i2c_async *xfer;
  int ret;

  for (;;) {
    pbl_irq_lock();

    if (state->busy) {
      pbl_irq_unlock();
      return;
    }

    xfer = state->head;
    if (xfer == nullptr) {
      if (state->powered) {
        bus->ops->disable(bus);
        state->powered = false;
      }
      pbl_irq_unlock();
      return;
    }

    if (!state->powered) {
      bus->ops->enable(bus);
      state->powered = true;
    }

    ret = bus->ops->start(bus, xfer->dev->addr, xfer->msgs, xfer->num_msgs);
    if (ret == 0) {
      state->busy = true;
      pbl_timer_start(&state->timer, PBL_TICKS(pbl_ms_to_ticks(TRANSFER_TIMEOUT_MS)));
      pbl_irq_unlock();
      return;
    }

    state->head = xfer->next;
    pbl_irq_unlock();

    xfer->done(xfer, ret);
  }
}

static void prv_finish(const struct pbl_i2c_bus *bus, int status) {
  struct pbl_i2c_bus_state *state = bus->state;
  struct pbl_i2c_async *xfer;

  pbl_irq_lock();
  if (!state->busy) {
    pbl_irq_unlock();
    return;
  }

  pbl_timer_stop(&state->timer);
  xfer = state->head;
  state->head = xfer->next;
  state->busy = false;
  pbl_irq_unlock();

  xfer->done(xfer, status);

  prv_kick(bus);
}

static void prv_timeout(struct pbl_timer *timer) {
  struct pbl_i2c_bus_state *state = PBL_CONTAINER_OF(timer, struct pbl_i2c_bus_state, timer);

  pbl_irq_lock();
  if (state->busy) {
    state->bus->ops->abort(state->bus);
  }
  pbl_irq_unlock();

  prv_finish(state->bus, -ETIMEDOUT);
}

static void prv_sync_done(struct pbl_i2c_async *xfer, int status) {
  struct pbl_i2c_bus_state *state = PBL_CONTAINER_OF(xfer, struct pbl_i2c_bus_state, sync);

  state->status = status;
  pbl_sem_give(&state->done);
}

int pbl_i2c_bus_dev_init(const struct pbl_device *dev) {
  const struct pbl_i2c_bus *bus = PBL_CONTAINER_OF(dev, const struct pbl_i2c_bus, dev);

  *bus->state = (struct pbl_i2c_bus_state){.bus = bus};
  pbl_sem_init(&bus->state->done, 0, 1);
  pbl_mutex_init(&bus->state->mutex);
  pbl_timer_init(&bus->state->timer, prv_timeout);

  return bus->ops->init(bus);
}

void pbl_i2c_bus_complete(const struct pbl_i2c_bus *bus, int status) {
  prv_finish(bus, status);
}

static int prv_validate(const struct pbl_i2c_msg *msgs, uint8_t num_msgs) {
  if (num_msgs == 0U || (msgs[0].flags & PBL_I2C_MSG_NOSTART) != 0U) {
    return -EINVAL;
  }

  for (uint8_t i = 0U; i < num_msgs; i++) {
    if ((msgs[i].flags & PBL_I2C_MSG_DMA_SAFE) != 0U &&
        ((uintptr_t)msgs[i].buf & (PBL_DCACHE_LINE_SIZE - 1U)) != 0U) {
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

int pbl_i2c_transfer_async(struct pbl_i2c_async *xfer) {
  const struct pbl_i2c_bus *bus = xfer->dev->bus;
  struct pbl_i2c_bus_state *state = bus->state;
  int ret;

  if (!pbl_device_is_ready(&bus->dev)) {
    return -ENODEV;
  }

  ret = prv_validate(xfer->msgs, xfer->num_msgs);
  if (ret != 0) {
    return ret;
  }

  xfer->next = nullptr;

  pbl_irq_lock();
  if (state->head == nullptr) {
    state->head = xfer;
  } else {
    state->tail->next = xfer;
  }
  state->tail = xfer;
  pbl_irq_unlock();

  prv_kick(bus);

  return 0;
}

int pbl_i2c_transfer(const struct pbl_i2c_dev *dev, struct pbl_i2c_msg *msgs, uint8_t num_msgs) {
  const struct pbl_i2c_bus *bus = dev->bus;
  struct pbl_i2c_bus_state *state = bus->state;
  int ret;

  pbl_mutex_lock(&state->mutex, PBL_FOREVER);

  state->sync = (struct pbl_i2c_async){
    .dev = dev,
    .msgs = msgs,
    .num_msgs = num_msgs,
    .done = prv_sync_done,
  };

  ret = pbl_i2c_transfer_async(&state->sync);
  if (ret == 0) {
    (void)pbl_sem_take(&state->done, PBL_FOREVER);
    ret = state->status;
  }

  pbl_mutex_unlock(&state->mutex);

  if (ret != 0) {
    PBL_ANALYTICS_ADD(i2c_transfer_error_count, 1);
    PBL_LOG_DBG("Transfer to 0x%02x on %s failed (%d)", dev->addr, pbl_device_name(&bus->dev), ret);
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
