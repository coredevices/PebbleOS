/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <pbl/device.h>
#include <pbl/drivers/gpio.h>
#include <pbl/kernel/sem.h>
#include <pbl/util/bits.h>
#include <pbl/util/size.h>

/**
 * @defgroup drivers_spi SPI
 * @ingroup drivers
 * @brief SPI controller class.
 *
 * An SPI controller is a struct pbl_spi_bus device with a list of chip select GPIOs, and a
 * peripheral on it a struct pbl_spi_dev: bus, chip select index, maximum clock frequency and mode.
 * A transfer is a list of full-duplex transfers sent with the chip select asserted throughout;
 * each one writes @c tx_buf, or zeros if it is NULL, and reads into @c rx_buf unless it is NULL.
 * As in Linux, chip selects are active low unless the device sets @ref PBL_SPI_CS_HIGH, whatever
 * the polarity flag of the chip select GPIO. A consumer calls pbl_spi_setup() once, before its
 * first transfer, which also drives the chip select inactive:
 *
 * @code{.c}
 * static const struct pbl_spi_dev s_lcd =
 *     PBL_SPI_DEV(&s_spi3.bus, 0, 1000000, PBL_SPI_MODE_0 | PBL_SPI_CS_HIGH);
 *
 * ret = pbl_spi_setup(&s_lcd);
 * if (ret != 0) {
 *   return ret;
 * }
 *
 * uint8_t cmd[] = {0x04, 0x00};
 * struct pbl_spi_xfer xfer = {.tx_buf = cmd, .len = sizeof(cmd)};
 *
 * return pbl_spi_transfer(&s_lcd, &xfer, 1U);
 * @endcode
 *
 * pbl_spi_transfer() blocks the calling task until the transfer is done. pbl_spi_transfer_async()
 * returns once it has started and reports the outcome to a callback run from the controller's
 * interrupt; the transfers and their buffers must stay valid until then. Transfers are serialized
 * per bus, must not be issued from ISRs, and the controller is powered for the duration of each.
 *
 * Calls return 0 or a negative errno: @c -EINVAL for an invalid transfer or one the controller
 * cannot issue, e.g. a buffer it cannot reach, and @c -ENODEV if the bus is not ready.
 * @{
 */

/**
 * @name Mode flags
 * @{
 */
/** Sample on the trailing clock edge. */
#define PBL_SPI_CPHA PBL_BIT(0)
/** Clock idles high. */
#define PBL_SPI_CPOL PBL_BIT(1)
/** Mode 0: clock idles low, sample on the leading edge. */
#define PBL_SPI_MODE_0 0U
/** Mode 1: clock idles low, sample on the trailing edge. */
#define PBL_SPI_MODE_1 PBL_SPI_CPHA
/** Mode 2: clock idles high, sample on the leading edge. */
#define PBL_SPI_MODE_2 PBL_SPI_CPOL
/** Mode 3: clock idles high, sample on the trailing edge. */
#define PBL_SPI_MODE_3 (PBL_SPI_CPOL | PBL_SPI_CPHA)
/** Chip select is active high. */
#define PBL_SPI_CS_HIGH PBL_BIT(2)
/** Send the least significant bit of each byte first. */
#define PBL_SPI_LSB_FIRST PBL_BIT(3)
/** @} */

/** @brief A full-duplex transfer. */
struct pbl_spi_xfer {
  /** Data to write, or NULL to write zeros. */
  const void *tx_buf;
  /** Buffer to read into, or NULL to discard the data read. */
  void *rx_buf;
  /** Number of bytes, at least 1. */
  uint32_t len;
};

/**
 * @brief Completion callback of pbl_spi_transfer_async().
 *
 * Runs from the controller's interrupt, with the bus released.
 *
 * @param status 0 or a negative errno.
 * @param arg Callback argument.
 */
typedef void (*pbl_spi_callback_t)(int status, void *arg);

/** @brief Class of the SPI buses. */
extern const struct pbl_device_class pbl_spi_bus_class;

struct pbl_spi_bus;
struct pbl_spi_dev;

/** @cond INTERNAL_HIDDEN */
struct pbl_spi_bus_state {
  struct pbl_sem lock;
  const struct pbl_spi_dev *dev;
  const struct pbl_spi_xfer *xfers;
  size_t num_xfers;
  size_t idx;
  pbl_spi_callback_t cb;
  void *arg;
};
/** @endcond */

/**
 * @brief Bus driver operations.
 *
 * Called with the bus lock held. The class layer owns locking, power, chip selects and the
 * sequence of transfers; the driver moves one transfer over the wire and reports its outcome with
 * pbl_spi_bus_complete().
 */
struct pbl_spi_bus_ops {
  /** Configure the controller, leaving it disabled. Returns 0 or a negative errno. */
  int (*init)(const struct pbl_spi_bus *bus);
  /**
   * Set the controller up for @p dev: the highest supported clock frequency up to
   * @c dev->max_hz and the mode. Called before enable(). Returns 0 or a negative errno.
   */
  int (*configure)(const struct pbl_spi_bus *bus, const struct pbl_spi_dev *dev);
  /** Power the controller up. */
  void (*enable)(const struct pbl_spi_bus *bus);
  /** Power the controller down. May be called from the controller's interrupt. */
  void (*disable)(const struct pbl_spi_bus *bus);
  /**
   * Start a transfer. Returns 0 if pbl_spi_bus_complete() will be called, or a negative errno.
   * May be called from the controller's interrupt.
   */
  int (*start)(const struct pbl_spi_bus *bus, const struct pbl_spi_xfer *xfer);
};

/** @brief An SPI controller. */
struct pbl_spi_bus {
  /** Device. */
  struct pbl_device dev;
  /** Driver operations. */
  const struct pbl_spi_bus_ops *ops;
  /** Runtime state. */
  struct pbl_spi_bus_state *state;
  /** Chip select GPIOs, set from the controller's interrupt; unconnected ones are not driven. */
  const struct pbl_gpio *cs_gpios;
  /** Number of chip selects. */
  uint8_t num_cs;
};

/** @brief A peripheral on a bus. */
struct pbl_spi_dev {
  /** Bus. */
  const struct pbl_spi_bus *bus;
  /** Chip select index. */
  uint8_t cs;
  /** Mode flags. */
  uint32_t mode;
  /** Maximum clock frequency, in Hz. */
  uint32_t max_hz;
};

/**
 * @brief Initializer for a struct pbl_spi_dev.
 *
 * @param _bus Bus.
 * @param _cs Chip select index.
 * @param _max_hz Maximum clock frequency, in Hz.
 * @param _mode Mode flags.
 */
#define PBL_SPI_DEV(_bus, _cs, _max_hz, _mode) \
  {.bus = (_bus), .cs = (_cs), .mode = (_mode), .max_hz = (_max_hz)}

/**
 * @brief Define the class state of bus @p sym, for the @c PBL_SPI_*_DEFINE() macro of a driver.
 *
 * @param sym Symbol of the bus instance.
 * @param ... Chip select GPIOs, from PBL_GPIO().
 */
#define PBL_SPI_BUS_STATE_DEFINE(sym, ...)         \
  PBL_DEVICE_STATE_DEFINE(sym);                    \
  static struct pbl_spi_bus_state sym##_spi_state; \
  static const struct pbl_gpio sym##_cs_gpios[] = {__VA_ARGS__}

/**
 * @brief Initializer for the struct pbl_spi_bus of bus @p sym.
 *
 * @param sym Symbol of the bus instance, with its state defined by PBL_SPI_BUS_STATE_DEFINE().
 * @param _name Name.
 * @param _ops Driver operations.
 * @param _deps Dependencies from PBL_DEVICE_DEPS(), or NULL.
 */
#define PBL_SPI_BUS_INIT(sym, _name, _ops, _deps)                                                \
  {                                                                                              \
    .dev =                                                                                       \
        PBL_DEVICE_INIT_CLASS(sym, _name, pbl_spi_bus_init, nullptr, _deps, &pbl_spi_bus_class), \
    .ops = (_ops),                                                                               \
    .state = &sym##_spi_state,                                                                   \
    .cs_gpios = sym##_cs_gpios,                                                                  \
    .num_cs = ARRAY_LENGTH(sym##_cs_gpios),                                                      \
  }

/**
 * @brief Device init of every bus: sets the class state up, then calls the driver's init.
 *
 * @param dev Bus device.
 * @return 0 or a negative errno.
 */
int pbl_spi_bus_init(const struct pbl_device *dev);

/**
 * @brief Report the outcome of the transfer in flight.
 *
 * Called by the bus driver from its interrupt handler. Starts the next transfer, or ends the
 * sequence and calls its callback.
 *
 * @param bus Bus.
 * @param status 0 or a negative errno.
 */
void pbl_spi_bus_complete(const struct pbl_spi_bus *bus, int status);

/**
 * @brief Set a device up: checks its chip select and mode, and drives its chip select inactive.
 *
 * @param dev Device.
 * @return 0 or a negative errno.
 */
int pbl_spi_setup(const struct pbl_spi_dev *dev);

/**
 * @brief Run transfers with a device, returning once they have started.
 *
 * @param dev Device.
 * @param xfers Transfers, valid until @p cb is called.
 * @param num_xfers Number of transfers.
 * @param cb Completion callback.
 * @param arg Callback argument.
 * @return 0 if @p cb will be called, or a negative errno.
 */
int pbl_spi_transfer_async(const struct pbl_spi_dev *dev, const struct pbl_spi_xfer *xfers,
                           size_t num_xfers, pbl_spi_callback_t cb, void *arg);

/**
 * @brief Run transfers with a device.
 *
 * @param dev Device.
 * @param xfers Transfers.
 * @param num_xfers Number of transfers.
 * @return 0 or a negative errno.
 */
int pbl_spi_transfer(const struct pbl_spi_dev *dev, const struct pbl_spi_xfer *xfers,
                     size_t num_xfers);

/**
 * @brief Write data.
 *
 * @param dev Device.
 * @param buf Data.
 * @param len Number of bytes.
 * @return 0 or a negative errno.
 */
static inline int pbl_spi_write(const struct pbl_spi_dev *dev, const void *buf, uint32_t len) {
  struct pbl_spi_xfer xfer = {.tx_buf = buf, .len = len};

  return pbl_spi_transfer(dev, &xfer, 1U);
}

/**
 * @brief Read data, writing zeros.
 *
 * @param dev Device.
 * @param[out] buf Buffer.
 * @param len Number of bytes.
 * @return 0 or a negative errno.
 */
static inline int pbl_spi_read(const struct pbl_spi_dev *dev, void *buf, uint32_t len) {
  struct pbl_spi_xfer xfer = {.rx_buf = buf, .len = len};

  return pbl_spi_transfer(dev, &xfer, 1U);
}

/**
 * @brief Write data, then read data.
 *
 * @param dev Device.
 * @param wbuf Data to write.
 * @param wlen Number of bytes to write.
 * @param[out] rbuf Buffer to read into.
 * @param rlen Number of bytes to read.
 * @return 0 or a negative errno.
 */
static inline int pbl_spi_write_read(const struct pbl_spi_dev *dev, const void *wbuf, uint32_t wlen,
                                     void *rbuf, uint32_t rlen) {
  struct pbl_spi_xfer xfers[] = {
    {.tx_buf = wbuf, .len = wlen},
    {.rx_buf = rbuf, .len = rlen},
  };

  return pbl_spi_transfer(dev, xfers, 2U);
}

/** @} */
