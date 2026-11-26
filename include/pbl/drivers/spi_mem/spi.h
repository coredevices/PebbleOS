/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/spi.h>
#include <pbl/drivers/spi_mem.h>
#include <pbl/kernel/mutex.h>

/**
 * @defgroup drivers_spi_mem_spi SPI memory on a SPI bus
 * @ingroup drivers_spi_mem
 * @brief Runs single-line memory operations as transfers on a SPI controller.
 *
 * A memory on a SPI controller with no memory operations of its own gets one of these: the
 * command, address and dummy bytes go out in a first transfer, and the data in a second, with
 * the chip select asserted throughout.
 * @{
 */

/** @brief Largest data phase, and the size of the buffer data to write goes through. */
#define PBL_SPI_MEM_SPI_BUF_SIZE 256U

/** @cond INTERNAL_HIDDEN */
struct pbl_spi_mem_spi_data {
  struct pbl_mutex lock;
  uint8_t hdr[1U + 4U + 8U];
  uint8_t buf[PBL_SPI_MEM_SPI_BUF_SIZE];
};
/** @endcond */

/** @brief SPI memory on a SPI bus. */
struct pbl_spi_mem_spi {
  /** Controller. */
  struct pbl_spi_mem mem;
  /** The memory on the bus. */
  struct pbl_spi_dev dev;
  /** Driver state. */
  struct pbl_spi_mem_spi_data *data;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_spi_mem_ops pbl_spi_mem_spi_ops;
/** @endcond */

/**
 * @brief Define a SPI memory on a SPI bus.
 *
 * @param sym Symbol of the device.
 * @param _name Name.
 * @param _bus SPI bus.
 * @param _cs Chip select of the memory.
 * @param _max_hz Highest SCK frequency of the memory, in Hz.
 */
#define PBL_SPI_MEM_SPI_DEFINE(sym, _name, _bus, _cs, _max_hz)                     \
  PBL_SPI_MEM_STATE_DEFINE(sym);                                                   \
  static struct pbl_spi_mem_spi_data sym##_data = {.lock = PBL_MUTEX_INITIALIZER}; \
  PBL_DEVICE_INSTANCE(sym, struct pbl_spi_mem_spi, struct pbl_spi_mem, mem) {      \
    .mem = PBL_SPI_MEM_INIT(sym, _name, nullptr, &pbl_spi_mem_spi_ops, 0U),        \
    .dev = PBL_SPI_DEV(_bus, _cs, _max_hz, PBL_SPI_MODE_0), .data = &sym##_data,   \
  }

/** @} */
