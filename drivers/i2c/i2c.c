/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/drivers/i2c.h>
#include <pbl/kernel/mutex.h>
#include <pbl/kernel/sem.h>
#include <pbl/kernel/types.h>
#include <pbl/logging/logging.h>
#include <pbl/mcu/cache.h>
#include <pbl/services/analytics/analytics.h>
#include <pbl/util/misc.h>

#include <kernel/util/sleep.h>
#include <system/passert.h>

PBL_LOG_MODULE_DEFINE(driver_i2c, CONFIG_DRIVER_I2C_LOG_LEVEL);

#define I2C_ERROR_TIMEOUT_MS (1000)

// MFI NACKs while busy. We delay ~1ms between retries so this is approximately a 1000ms timeout.
// The longest operation of the MFi chip is "start signature generation", which seems to take
// 223-224 NACKs, but sometimes for unknown reasons it can take much longer.
#define I2C_NACK_COUNT_MAX (1000)

static const char *prv_name(const struct pbl_i2c_bus *bus) {
  return bus->dev.name;
}

//! Caller must hold the bus mutex
static void prv_bus_reset(const struct pbl_i2c_bus *bus) {
  bus->ops->disable(bus);
  bus->ops->enable(bus);
}

int pbl_i2c_bus_init(const struct pbl_device *dev) {
  const struct pbl_i2c_bus *bus = container_of(dev, const struct pbl_i2c_bus, dev);

  *bus->state = (struct pbl_i2c_bus_state){0};
  pbl_sem_init(&bus->state->event_sem, 1, 1);
  pbl_mutex_init(&bus->state->mutex);

  return bus->ops->init(bus);
}

void pbl_i2c_bus_event(const struct pbl_i2c_bus *bus, enum pbl_i2c_event event) {
  bus->state->event = event;
  pbl_sem_give(&bus->state->event_sem);
}

void pbl_i2c_use(const struct pbl_i2c_dev *dev) {
  const struct pbl_i2c_bus *bus = dev->bus;

  PBL_ASSERTN(pbl_device_is_ready(&bus->dev));

  pbl_mutex_lock(&bus->state->mutex, PBL_FOREVER);

  if (bus->state->user_count == 0) {
    bus->ops->enable(bus);
  }
  bus->state->user_count++;

  pbl_mutex_unlock(&bus->state->mutex);
}

void pbl_i2c_release(const struct pbl_i2c_dev *dev) {
  const struct pbl_i2c_bus *bus = dev->bus;

  pbl_mutex_lock(&bus->state->mutex, PBL_FOREVER);

  if (bus->state->user_count == 0) {
    PBL_LOG_ERR("Attempted release of disabled bus %s", prv_name(bus));
    pbl_mutex_unlock(&bus->state->mutex);
    return;
  }

  bus->state->user_count--;
  if (bus->state->user_count == 0) {
    bus->ops->disable(bus);
  }

  pbl_mutex_unlock(&bus->state->mutex);
}

//! Wait a short amount of time for the busy flag to clear
static bool prv_wait_for_not_busy(const struct pbl_i2c_bus *bus) {
  static const int WAIT_DELAY = 10; // milliseconds

  if (bus->ops->is_busy(bus)) {
    psleep(WAIT_DELAY);
    if (bus->ops->is_busy(bus)) {
      PBL_LOG_ERR("Timed out waiting for bus %s to become non-busy", prv_name(bus));
      return false;
    }
  }

  return true;
}

//! Set up and start a transfer, wait for it to finish and clean up after it.
//! Caller must hold the bus mutex
static bool prv_do_transfer_locked(const struct pbl_i2c_bus *bus,
                                   const struct pbl_i2c_transfer *transfer) {
  struct pbl_i2c_bus_state *state = bus->state;

  if (state->user_count == 0) {
    PBL_LOG_ERR("Attempted access to disabled bus %s", prv_name(bus));
    return false;
  }

  // The bus should not be busy, as every transfer waits for it to become idle before returning.
  // If it is, reset it, and give up if that does not help.
  if (bus->ops->is_busy(bus)) {
    prv_bus_reset(bus);

    if (!prv_wait_for_not_busy(bus)) {
      PBL_LOG_ERR("I2C bus did not recover after reset (%s)", prv_name(bus));
      return false;
    }
  }

  // Take the token so that the next take blocks until the transfer ends
  PBL_ASSERT(pbl_sem_take(&state->event_sem, PBL_NO_WAIT) == 0,
             "Could not acquire semaphore token");

  state->transfer = *transfer;
  state->nack_count = 0;

  if (bus->ops->begin_transfer != NULL) {
    bus->ops->begin_transfer(bus);
  }

  bool result = false;
  bool complete = false;
  do {
    bus->ops->start_transfer(bus);

    if (pbl_sem_take(&state->event_sem, PBL_TICKS(pbl_ms_to_ticks(I2C_ERROR_TIMEOUT_MS))) == 0) {
      if ((state->event == PBL_I2C_EVENT_COMPLETE) || (state->event == PBL_I2C_EVENT_ERROR)) {
        if (state->event == PBL_I2C_EVENT_ERROR) {
          PBL_LOG_ERR("I2C Error on bus %s", prv_name(bus));
          PBL_ANALYTICS_ADD(i2c_transfer_error_count, 1);
        }
        complete = true;
        result = (state->event == PBL_I2C_EVENT_COMPLETE);
      } else if (state->nack_count < I2C_NACK_COUNT_MAX) {
        // NACK received after the start condition: the MFI chip NACKs start conditions while it
        // is busy, so retry after a short delay. Legitimate NACKs abort the transfer once the
        // NACK count reaches its maximum.
        state->nack_count++;
        psleep(2);
      } else {
        bus->ops->abort_transfer(bus);
        complete = true;
        PBL_LOG_ERR("I2C Error: too many NACKs received on bus %s", prv_name(bus));
        break;
      }
    } else {
      bus->ops->abort_transfer(bus);
      complete = true;
      PBL_LOG_ERR("Transfer timed out on bus %s", prv_name(bus));
      PBL_ANALYTICS_ADD(i2c_transfer_error_count, 1);
      break;
    }
  } while (!complete);

  // Return the token so another transfer can be started
  pbl_sem_give(&state->event_sem);

  // A transfer could complete successfully while the busy flag never clears, which would make the
  // next transfer fail: reset the bus if that happens
  if (!prv_wait_for_not_busy(bus)) {
    prv_bus_reset(bus);
  }

  return result;
}

static bool prv_do_transfer(const struct pbl_i2c_dev *dev, struct pbl_i2c_transfer *transfer) {
  const struct pbl_i2c_bus *bus = dev->bus;

  transfer->addr = dev->addr;

  pbl_mutex_lock(&bus->state->mutex, PBL_FOREVER);
  bool result = prv_do_transfer_locked(bus, transfer);
  pbl_mutex_unlock(&bus->state->mutex);

  if (!result) {
    PBL_LOG_ERR("%s failed on bus %s", (transfer->dir == PBL_I2C_READ) ? "Read" : "Write",
                prv_name(bus));
  }

  return result;
}

bool pbl_i2c_read_register(const struct pbl_i2c_dev *dev, uint8_t reg, uint8_t *result) {
  return pbl_i2c_read_register_block(dev, reg, 1, result);
}

bool pbl_i2c_read_register_block(const struct pbl_i2c_dev *dev, uint8_t reg, uint32_t size,
                                 uint8_t *result) {
  PBL_ASSERTN(result);

  struct pbl_i2c_transfer transfer = {
    .dir = PBL_I2C_READ,
    .with_reg = true,
    .reg = reg,
    .size = size,
    .data = result,
  };

  return prv_do_transfer(dev, &transfer);
}

bool pbl_i2c_read_register_block_dma(const struct pbl_i2c_dev *dev, uint8_t reg, uint32_t size,
                                     uint8_t *result) {
  PBL_ASSERTN(((uintptr_t)result & (DCACHE_LINE_SIZE_MAX - 1U)) == 0U);

  struct pbl_i2c_transfer transfer = {
    .dir = PBL_I2C_READ,
    .with_reg = true,
    .reg = reg,
    .size = size,
    .data = result,
    .dma = true,
  };

  return prv_do_transfer(dev, &transfer);
}

bool pbl_i2c_write_register(const struct pbl_i2c_dev *dev, uint8_t reg, uint8_t value) {
  return pbl_i2c_write_register_block(dev, reg, 1, &value);
}

bool pbl_i2c_write_register_block(const struct pbl_i2c_dev *dev, uint8_t reg, uint32_t size,
                                  const uint8_t *data) {
  PBL_ASSERTN(data);

  struct pbl_i2c_transfer transfer = {
    .dir = PBL_I2C_WRITE,
    .with_reg = true,
    .reg = reg,
    .size = size,
    .data = (uint8_t *)data,
  };

  return prv_do_transfer(dev, &transfer);
}

bool pbl_i2c_read_block(const struct pbl_i2c_dev *dev, uint32_t size, uint8_t *result) {
  PBL_ASSERTN(result);

  struct pbl_i2c_transfer transfer = {
    .dir = PBL_I2C_READ,
    .size = size,
    .data = result,
  };

  return prv_do_transfer(dev, &transfer);
}

bool pbl_i2c_write_block(const struct pbl_i2c_dev *dev, uint32_t size, const uint8_t *data) {
  PBL_ASSERTN(data);

  struct pbl_i2c_transfer transfer = {
    .dir = PBL_I2C_WRITE,
    .size = size,
    .data = (uint8_t *)data,
  };

  return prv_do_transfer(dev, &transfer);
}

bool pbl_i2c_write_read_block(const struct pbl_i2c_dev *dev, uint32_t write_size,
                              const uint8_t *write_data, uint32_t read_size, uint8_t *read_data) {
  PBL_ASSERTN(write_data);
  PBL_ASSERTN(read_data);

  const struct pbl_i2c_bus *bus = dev->bus;
  const struct pbl_i2c_transfer write = {
    .addr = dev->addr,
    .dir = PBL_I2C_WRITE,
    .size = write_size,
    .data = (uint8_t *)write_data,
  };
  const struct pbl_i2c_transfer read = {
    .addr = dev->addr,
    .dir = PBL_I2C_READ,
    .size = read_size,
    .data = read_data,
  };

  pbl_mutex_lock(&bus->state->mutex, PBL_FOREVER);
  bool result = prv_do_transfer_locked(bus, &write) && prv_do_transfer_locked(bus, &read);
  pbl_mutex_unlock(&bus->state->mutex);

  if (!result) {
    PBL_LOG_ERR("Write-read block failed on bus %s", prv_name(bus));
  }

  return result;
}
