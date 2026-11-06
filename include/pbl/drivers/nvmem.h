/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <pbl/device.h>

/**
 * @defgroup drivers_nvmem Non-volatile memory
 * @ingroup drivers
 * @brief Non-volatile memory class.
 *
 * Small non-volatile memories holding data the rest of the system consumes, such as eFuses. A
 * provider exposes the memory as a flat byte address space. A consumer holds a struct
 * pbl_nvmem_cell, what a devicetree @c nvmem-cells entry describes: a named data item at a fixed
 * location in the memory.
 *
 * @code{.c}
 * static const struct pbl_nvmem_cell s_mac = PBL_NVMEM_CELL(&s_efuse, 0x20U, 8U);
 *
 * uint8_t mac[8];
 * ret = pbl_nvmem_cell_read(&s_mac, mac, sizeof(mac));
 * @endcode
 * @{
 */

struct pbl_nvmem;

/** @brief Class of the non-volatile memories. */
extern const struct pbl_device_class pbl_nvmem_class;

/** @brief Provider operations. */
struct pbl_nvmem_ops {
  /**
   * Read from the memory.
   *
   * The class has checked the access against the size, word size and stride of the memory.
   *
   * @param nvmem Memory.
   * @param offset Offset in bytes.
   * @param[out] val Data.
   * @param bytes Number of bytes.
   * @return 0 on success or a negative errno.
   */
  int (*reg_read)(const struct pbl_nvmem *nvmem, unsigned int offset, void *val, size_t bytes);
};

/** @brief Non-volatile memory. */
struct pbl_nvmem {
  /** Device. */
  struct pbl_device dev;
  /** Operations. */
  const struct pbl_nvmem_ops *ops;
  /** Size in bytes. */
  size_t size;
  /** Minimum read size in bytes; reads are a multiple of it. */
  uint8_t word_size;
  /** Minimum read alignment in bytes. */
  uint8_t stride;
};

/**
 * @brief Initializer of a struct pbl_nvmem, for providers.
 *
 * @param sym Memory symbol, with its PBL_DEVICE_STATE_DEFINE().
 * @param _name Device name.
 * @param _init Device init function, or NULL.
 * @param _deps Device dependencies from PBL_DEVICE_DEPS(), or NULL.
 * @param _ops Operations.
 * @param _size Size in bytes.
 * @param _word_size Minimum read size in bytes.
 * @param _stride Minimum read alignment in bytes.
 */
#define PBL_NVMEM_INIT(sym, _name, _init, _deps, _ops, _size, _word_size, _stride)     \
  {                                                                                    \
    .dev = PBL_DEVICE_INIT_CLASS(sym, _name, _init, nullptr, _deps, &pbl_nvmem_class), \
    .ops = (_ops),                                                                     \
    .size = (_size),                                                                   \
    .word_size = (_word_size),                                                         \
    .stride = (_stride),                                                               \
  }

/** @brief A data item in a memory, as used by a consumer. */
struct pbl_nvmem_cell {
  /** Memory. */
  const struct pbl_nvmem *nvmem;
  /** Offset in bytes. */
  unsigned int offset;
  /** Size in bytes. */
  size_t bytes;
};

/**
 * @brief Initializer of a struct pbl_nvmem_cell.
 *
 * @param _nvmem Memory.
 * @param _offset Offset in bytes.
 * @param _bytes Size in bytes.
 */
#define PBL_NVMEM_CELL(_nvmem, _offset, _bytes) \
  {                                             \
    .nvmem = (_nvmem),                          \
    .offset = (_offset),                        \
    .bytes = (_bytes),                          \
  }

/**
 * @brief Read a cell.
 *
 * @param cell Cell.
 * @param[out] buf Data.
 * @param len Size of @p buf in bytes, at least the size of the cell.
 * @return 0 on success, -ENODEV if the memory is not ready, -EINVAL if @p buf is too small or the
 *         cell does not fit the memory's geometry, or a negative errno from the provider.
 */
int pbl_nvmem_cell_read(const struct pbl_nvmem_cell *cell, void *buf, size_t len);

/**
 * @brief Read raw bytes from a memory.
 *
 * @param nvmem Memory.
 * @param offset Offset in bytes, a multiple of the stride.
 * @param bytes Number of bytes, a multiple of the word size.
 * @param[out] buf Data.
 * @return 0 on success, -ENODEV if the memory is not ready, -EINVAL if the access does not fit the
 *         memory's geometry, or a negative errno from the provider.
 */
int pbl_nvmem_device_read(const struct pbl_nvmem *nvmem, unsigned int offset, size_t bytes,
                          void *buf);

/** @} */
