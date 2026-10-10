/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <pbl/device.h>
#include <pbl/kernel/mutex.h>
#include <pbl/kernel/sem.h>

/**
 * @defgroup drivers_i2c I2C
 * @ingroup drivers
 * @brief I2C controller class.
 *
 * An I2C controller is a struct pbl_i2c_bus device, and a peripheral on it a struct pbl_i2c_dev:
 * bus plus 7-bit address. A bus is powered while it has users: bracket transfers with
 * pbl_i2c_use() and pbl_i2c_release(). Transfers block the calling task, are serialized per bus
 * and must not be issued from ISRs.
 *
 * @code{.c}
 * static const struct pbl_i2c_dev s_accel = PBL_I2C_DEV(&s_i2c2.bus, 0x6a);
 *
 * uint8_t id;
 * uint8_t fifo[6];
 *
 * pbl_i2c_use(&s_accel);
 * bool ok = pbl_i2c_read_register(&s_accel, REG_WHO_AM_I, &id) &&
 *           pbl_i2c_write_register(&s_accel, REG_CTRL1, CTRL1_ODR_50HZ) &&
 *           pbl_i2c_read_register_block(&s_accel, REG_OUT_X_L, sizeof(fifo), fifo);
 * pbl_i2c_release(&s_accel);
 * @endcode
 * @{
 */

/** @brief Transfer outcome reported by a bus driver. */
enum pbl_i2c_event {
  /** Transfer timed out. */
  PBL_I2C_EVENT_TIMEOUT,
  /** Transfer completed. */
  PBL_I2C_EVENT_COMPLETE,
  /** Device did not acknowledge; the transfer is retried. */
  PBL_I2C_EVENT_NACK,
  /** Transfer failed. */
  PBL_I2C_EVENT_ERROR,
};

/** @brief Transfer direction. */
enum pbl_i2c_dir {
  /** Read from the device. */
  PBL_I2C_READ,
  /** Write to the device. */
  PBL_I2C_WRITE,
};

/** @brief Transfer in progress on a bus. */
struct pbl_i2c_transfer {
  /** 7-bit device address. */
  uint16_t addr;
  /** Direction. */
  enum pbl_i2c_dir dir;
  /** Send @ref reg first; a read then continues after a repeated start. */
  bool with_reg;
  /** Register address, with @ref with_reg. */
  uint8_t reg;
  /** Number of data bytes. */
  uint32_t size;
  /** Data to write or buffer to read into. */
  uint8_t *data;
  /** @ref data follows the pbl_i2c_read_register_block_dma() rules, so DMA may be used. */
  bool dma;
};

/** @brief Bus runtime state, owned by the class layer. */
struct pbl_i2c_bus_state {
  /** Current transfer. */
  struct pbl_i2c_transfer transfer;
  /** Outcome of the current transfer. */
  enum pbl_i2c_event event;
  /** NACKs received during the current transfer. */
  int nack_count;
  /** Number of pbl_i2c_use() users. */
  int user_count;
  /** Signaled when the current transfer ends. */
  struct pbl_sem event_sem;
  /** Serializes bus access. */
  struct pbl_mutex mutex;
};

struct pbl_i2c_bus;

/**
 * @brief Bus driver operations.
 *
 * Called with the bus lock held. The class layer owns locking, retries and timeouts; the driver
 * moves the current transfer over the wire and reports its outcome with pbl_i2c_bus_event().
 */
struct pbl_i2c_bus_ops {
  /** Configure the controller, leaving it disabled. Returns 0 or a negative errno. */
  int (*init)(const struct pbl_i2c_bus *bus);
  /** Enable the controller. */
  void (*enable)(const struct pbl_i2c_bus *bus);
  /** Disable the controller. */
  void (*disable)(const struct pbl_i2c_bus *bus);
  /** Check whether the controller is busy. */
  bool (*is_busy)(const struct pbl_i2c_bus *bus);
  /** Optional. Prepare for the current transfer, before its first start_transfer(). */
  void (*begin_transfer)(const struct pbl_i2c_bus *bus);
  /** Start the current transfer; called again to retry after a NACK. */
  void (*start_transfer)(const struct pbl_i2c_bus *bus);
  /** Abort the current transfer. */
  void (*abort_transfer)(const struct pbl_i2c_bus *bus);
};

/** @brief An I2C controller. */
struct pbl_i2c_bus {
  /** Device. */
  struct pbl_device dev;
  /** Driver operations. */
  const struct pbl_i2c_bus_ops *ops;
  /** Runtime state. */
  struct pbl_i2c_bus_state *state;
};

/** @brief A peripheral on a bus. */
struct pbl_i2c_dev {
  /** Bus. */
  const struct pbl_i2c_bus *bus;
  /** 7-bit address. */
  uint16_t addr;
};

/**
 * @brief Initializer for a struct pbl_i2c_dev.
 *
 * @param _bus Bus.
 * @param _addr 7-bit address.
 */
#define PBL_I2C_DEV(_bus, _addr) {.bus = (_bus), .addr = (_addr)}

/**
 * @brief Define the class state of bus @p sym, for the @c PBL_I2C_*_DEFINE() macro of a driver.
 *
 * @param sym Symbol of the bus instance.
 */
#define PBL_I2C_BUS_STATE_DEFINE(sym) \
  PBL_DEVICE_STATE_DEFINE(sym);       \
  static struct pbl_i2c_bus_state sym##_i2c_state

/**
 * @brief Initializer for the struct pbl_i2c_bus of bus @p sym.
 *
 * @param sym Symbol of the bus instance, with its state defined by PBL_I2C_BUS_STATE_DEFINE().
 * @param _name Name.
 * @param _ops Driver operations.
 * @param _deps Dependencies from PBL_DEVICE_DEPS(), or NULL.
 */
#define PBL_I2C_BUS_INIT(sym, _name, _ops, _deps)                      \
  {                                                                    \
    .dev = PBL_DEVICE_INIT(sym, _name, pbl_i2c_bus_init, NULL, _deps), \
    .ops = (_ops),                                                     \
    .state = &sym##_i2c_state,                                         \
  }

/**
 * @brief Device init of every bus: sets the class state up, then calls the driver's init.
 *
 * @param dev Bus device.
 * @return 0 or a negative errno.
 */
int pbl_i2c_bus_init(const struct pbl_device *dev);

/**
 * @brief Report the outcome of the current transfer.
 *
 * Called by the bus driver, typically from its interrupt handler.
 *
 * @param bus Bus.
 * @param event Transfer outcome.
 */
void pbl_i2c_bus_event(const struct pbl_i2c_bus *bus, enum pbl_i2c_event event);

/**
 * @brief Start using the bus a device is on.
 *
 * Reference counted per bus; the bus is enabled on the first user. Must be called before any
 * transfer to the device.
 *
 * @param dev Device.
 */
void pbl_i2c_use(const struct pbl_i2c_dev *dev);

/**
 * @brief Stop using the bus a device is on.
 *
 * The bus is disabled when its last user releases it.
 *
 * @param dev Device.
 */
void pbl_i2c_release(const struct pbl_i2c_dev *dev);

/**
 * @brief Read a register.
 *
 * @param dev Device.
 * @param reg Register address.
 * @param[out] result Register value.
 * @return True on success.
 */
bool pbl_i2c_read_register(const struct pbl_i2c_dev *dev, uint8_t reg, uint8_t *result);

/**
 * @brief Read consecutive registers.
 *
 * Sends the start register address, then reads after a repeated start.
 *
 * @param dev Device.
 * @param reg First register address.
 * @param size Number of bytes to read.
 * @param[out] result Destination buffer of at least @p size bytes.
 * @return True on success.
 */
bool pbl_i2c_read_register_block(const struct pbl_i2c_dev *dev, uint8_t reg, uint32_t size,
                                 uint8_t *result);

/**
 * @brief Read consecutive registers into a buffer the controller may fill through DMA.
 *
 * Like pbl_i2c_read_register_block(), but the bus may use DMA and let the CPU sleep through the
 * transfer. The buffer must be aligned to @ref DCACHE_LINE_SIZE_MAX and own the whole cache lines
 * it covers, @c DCACHE_ROUND_UP(size) bytes: declare it with @c PBL_ALIGNED(DCACHE_LINE_SIZE_MAX)
 * and that size.
 *
 * @param dev Device.
 * @param reg First register address.
 * @param size Number of bytes to read.
 * @param[out] result Destination buffer, see above.
 * @return True on success.
 */
bool pbl_i2c_read_register_block_dma(const struct pbl_i2c_dev *dev, uint8_t reg, uint32_t size,
                                     uint8_t *result);

/**
 * @brief Write a register.
 *
 * @param dev Device.
 * @param reg Register address.
 * @param value Value to write.
 * @return True on success.
 */
bool pbl_i2c_write_register(const struct pbl_i2c_dev *dev, uint8_t reg, uint8_t value);

/**
 * @brief Write consecutive registers.
 *
 * @param dev Device.
 * @param reg First register address.
 * @param size Number of bytes to write.
 * @param data Data to write.
 * @return True on success.
 */
bool pbl_i2c_write_register_block(const struct pbl_i2c_dev *dev, uint8_t reg, uint32_t size,
                                  const uint8_t *data);

/**
 * @brief Read data without sending a register address.
 *
 * @param dev Device.
 * @param size Number of bytes to read.
 * @param[out] result Destination buffer of at least @p size bytes.
 * @return True on success.
 */
bool pbl_i2c_read_block(const struct pbl_i2c_dev *dev, uint32_t size, uint8_t *result);

/**
 * @brief Write data without sending a register address.
 *
 * @param dev Device.
 * @param size Number of bytes to write.
 * @param data Data to write.
 * @return True on success.
 */
bool pbl_i2c_write_block(const struct pbl_i2c_dev *dev, uint32_t size, const uint8_t *data);

/**
 * @brief Write data, then read data, with no other transfer on the bus in between.
 *
 * The read is skipped if the write fails.
 *
 * @param dev Device.
 * @param write_size Number of bytes to write.
 * @param write_data Data to write.
 * @param read_size Number of bytes to read.
 * @param[out] read_data Destination buffer of at least @p read_size bytes.
 * @return True if both transfers succeeded.
 */
bool pbl_i2c_write_read_block(const struct pbl_i2c_dev *dev, uint32_t write_size,
                              const uint8_t *write_data, uint32_t read_size, uint8_t *read_data);

/** @} */
