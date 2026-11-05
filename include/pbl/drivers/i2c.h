/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <pbl/device.h>
#include <pbl/kernel/mutex.h>
#include <pbl/kernel/sem.h>
#include <pbl/util/bits.h>

/**
 * @defgroup drivers_i2c I2C
 * @ingroup drivers
 * @brief I2C controller class.
 *
 * An I2C controller is a struct pbl_i2c_bus device, and a peripheral on it a struct pbl_i2c_dev:
 * bus plus 7-bit address. A transfer is a list of messages, each a read or a write, sent as one
 * bus transaction: a START, a repeated START between messages and a STOP after the last one. A
 * write flagged @ref PBL_I2C_MSG_NOSTART continues the previous write instead, e.g. data after a
 * register address. Transfers block the calling task, are serialized per bus and must not be
 * issued from ISRs. The controller is powered for the duration of each transfer.
 *
 * @code{.c}
 * static const struct pbl_i2c_dev s_accel = PBL_I2C_DEV(&s_i2c2.bus, 0x6a);
 *
 * uint8_t id;
 * uint8_t xyz[6];
 * int ret;
 *
 * ret = pbl_i2c_reg_read_byte(&s_accel, REG_WHO_AM_I, &id);
 * if (ret != 0) {
 *   return ret;
 * }
 *
 * return pbl_i2c_burst_read(&s_accel, REG_OUT_X_L, xyz, sizeof(xyz));
 * @endcode
 *
 * Calls return 0 or a negative errno: @c -ENXIO when the address is not acknowledged, @c -EIO on a
 * data NACK or a bus error, @c -ETIMEDOUT when the transfer does not finish, @c -ENOTSUP when the
 * controller cannot issue the message sequence and @c -EINVAL for an invalid one.
 * @{
 */

/**
 * @name Message flags
 * @{
 */
/** Write to the device. */
#define PBL_I2C_MSG_WRITE 0U
/** Read from the device. */
#define PBL_I2C_MSG_READ PBL_BIT(0)
/** Continue the previous write without a repeated START or address. Writes only. */
#define PBL_I2C_MSG_NOSTART PBL_BIT(1)
/**
 * The buffer may be accessed through DMA: aligned to @ref DCACHE_LINE_SIZE_MAX and owning the
 * whole cache lines it covers, @c DCACHE_ROUND_UP(len) bytes. Declare it with
 * @c PBL_ALIGNED(DCACHE_LINE_SIZE_MAX) and that size.
 */
#define PBL_I2C_MSG_DMA_SAFE PBL_BIT(2)
/** @} */

/** @brief A message of a transfer. */
struct pbl_i2c_msg {
  /** Data to write or buffer to read into. */
  uint8_t *buf;
  /** Number of bytes. */
  uint32_t len;
  /** Message flags. */
  uint32_t flags;
};

/** @brief Class of the I2C buses. */
extern const struct pbl_device_class pbl_i2c_bus_class;

/** @cond INTERNAL_HIDDEN */
struct pbl_i2c_bus_state {
  int status;
  struct pbl_sem done;
  struct pbl_mutex mutex;
};
/** @endcond */

struct pbl_i2c_bus;

/**
 * @brief Bus driver operations.
 *
 * Called with the bus lock held. The class layer owns locking, power and timeouts; the driver
 * moves a transfer over the wire and reports its outcome with pbl_i2c_bus_complete().
 */
struct pbl_i2c_bus_ops {
  /** Configure the controller, leaving it disabled. Returns 0 or a negative errno. */
  int (*init)(const struct pbl_i2c_bus *bus);
  /** Power the controller up. */
  void (*enable)(const struct pbl_i2c_bus *bus);
  /** Power the controller down. */
  void (*disable)(const struct pbl_i2c_bus *bus);
  /**
   * Start a transfer of @p num_msgs validated messages to @p addr. Returns 0 if
   * pbl_i2c_bus_complete() will be called, or a negative errno, e.g. @c -ENOTSUP for a sequence
   * the controller cannot issue.
   */
  int (*start)(const struct pbl_i2c_bus *bus, uint16_t addr, struct pbl_i2c_msg *msgs,
               uint8_t num_msgs);
  /** Stop the transfer in flight, which then never completes, and reset the controller. */
  void (*abort)(const struct pbl_i2c_bus *bus);
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
#define PBL_I2C_BUS_INIT(sym, _name, _ops, _deps)                                                \
  {                                                                                              \
    .dev =                                                                                       \
        PBL_DEVICE_INIT_CLASS(sym, _name, pbl_i2c_bus_init, nullptr, _deps, &pbl_i2c_bus_class), \
    .ops = (_ops),                                                                               \
    .state = &sym##_i2c_state,                                                                   \
  }

/**
 * @brief Device init of every bus: sets the class state up, then calls the driver's init.
 *
 * @param dev Bus device.
 * @return 0 or a negative errno.
 */
int pbl_i2c_bus_init(const struct pbl_device *dev);

/**
 * @brief Report the outcome of the transfer in flight.
 *
 * Called by the bus driver, typically from its interrupt handler.
 *
 * @param bus Bus.
 * @param status 0 or a negative errno, see @ref drivers_i2c.
 */
void pbl_i2c_bus_complete(const struct pbl_i2c_bus *bus, int status);

/**
 * @brief Transfer messages to or from a device.
 *
 * @param dev Device.
 * @param msgs Messages.
 * @param num_msgs Number of messages.
 * @return 0 or a negative errno.
 */
int pbl_i2c_transfer(const struct pbl_i2c_dev *dev, struct pbl_i2c_msg *msgs, uint8_t num_msgs);

/**
 * @brief Write data.
 *
 * @param dev Device.
 * @param buf Data.
 * @param len Number of bytes.
 * @return 0 or a negative errno.
 */
static inline int pbl_i2c_write(const struct pbl_i2c_dev *dev, const void *buf, uint32_t len) {
  struct pbl_i2c_msg msg = {.buf = (uint8_t *)buf, .len = len, .flags = PBL_I2C_MSG_WRITE};

  return pbl_i2c_transfer(dev, &msg, 1U);
}

/**
 * @brief Read data.
 *
 * @param dev Device.
 * @param[out] buf Buffer.
 * @param len Number of bytes.
 * @return 0 or a negative errno.
 */
static inline int pbl_i2c_read(const struct pbl_i2c_dev *dev, void *buf, uint32_t len) {
  struct pbl_i2c_msg msg = {.buf = buf, .len = len, .flags = PBL_I2C_MSG_READ};

  return pbl_i2c_transfer(dev, &msg, 1U);
}

/**
 * @brief Write data, then read data after a repeated START.
 *
 * @param dev Device.
 * @param wbuf Data to write.
 * @param wlen Number of bytes to write.
 * @param[out] rbuf Buffer to read into.
 * @param rlen Number of bytes to read.
 * @return 0 or a negative errno.
 */
static inline int pbl_i2c_write_read(const struct pbl_i2c_dev *dev, const void *wbuf, uint32_t wlen,
                                     void *rbuf, uint32_t rlen) {
  struct pbl_i2c_msg msgs[] = {
    {.buf = (uint8_t *)wbuf, .len = wlen, .flags = PBL_I2C_MSG_WRITE},
    {.buf = rbuf, .len = rlen, .flags = PBL_I2C_MSG_READ},
  };

  return pbl_i2c_transfer(dev, msgs, 2U);
}

/**
 * @brief Read consecutive registers.
 *
 * @param dev Device.
 * @param reg First register address.
 * @param[out] buf Buffer.
 * @param len Number of bytes.
 * @return 0 or a negative errno.
 */
static inline int pbl_i2c_burst_read(const struct pbl_i2c_dev *dev, uint8_t reg, void *buf,
                                     uint32_t len) {
  return pbl_i2c_write_read(dev, &reg, 1U, buf, len);
}

/**
 * @brief Write consecutive registers.
 *
 * @param dev Device.
 * @param reg First register address.
 * @param buf Data.
 * @param len Number of bytes.
 * @return 0 or a negative errno.
 */
static inline int pbl_i2c_burst_write(const struct pbl_i2c_dev *dev, uint8_t reg, const void *buf,
                                      uint32_t len) {
  struct pbl_i2c_msg msgs[] = {
    {.buf = &reg, .len = 1U, .flags = PBL_I2C_MSG_WRITE},
    {.buf = (uint8_t *)buf, .len = len, .flags = PBL_I2C_MSG_WRITE | PBL_I2C_MSG_NOSTART},
  };

  return pbl_i2c_transfer(dev, msgs, 2U);
}

/**
 * @brief Read a register.
 *
 * @param dev Device.
 * @param reg Register address.
 * @param[out] val Register value.
 * @return 0 or a negative errno.
 */
static inline int pbl_i2c_reg_read_byte(const struct pbl_i2c_dev *dev, uint8_t reg, uint8_t *val) {
  return pbl_i2c_burst_read(dev, reg, val, 1U);
}

/**
 * @brief Write a register.
 *
 * @param dev Device.
 * @param reg Register address.
 * @param val Value.
 * @return 0 or a negative errno.
 */
static inline int pbl_i2c_reg_write_byte(const struct pbl_i2c_dev *dev, uint8_t reg, uint8_t val) {
  uint8_t buf[] = {reg, val};

  return pbl_i2c_write(dev, buf, sizeof(buf));
}

/**
 * @brief Update bits of a register.
 *
 * Reads the register and writes it back, if it changes, with the @p mask bits set to @p val.
 * Not atomic with respect to other users of the device.
 *
 * @param dev Device.
 * @param reg Register address.
 * @param mask Bits to update.
 * @param val New value of the @p mask bits.
 * @return 0 or a negative errno.
 */
int pbl_i2c_reg_update_byte(const struct pbl_i2c_dev *dev, uint8_t reg, uint8_t mask, uint8_t val);

/** @} */
