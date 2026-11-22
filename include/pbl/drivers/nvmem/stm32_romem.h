/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/nvmem.h>

/**
 * @defgroup drivers_nvmem_stm32_romem STM32 factory-programmed data
 * @ingroup drivers_nvmem
 * @brief @ref drivers_nvmem provider of the read-only memory an STM32 holds its factory data
 * in, as the calibration of the analog blocks in the system memory of the STM32H7.
 * @{
 */

/** @brief Factory-programmed memory. */
struct pbl_nvmem_stm32_romem {
  /** NVMEM provider. */
  struct pbl_nvmem nvmem;
  /** Memory. */
  uintptr_t base;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_nvmem_ops pbl_nvmem_stm32_romem_ops;
/** @endcond */

/**
 * @brief Define the memory.
 *
 * @param sym Symbol.
 * @param _name Device name.
 * @param _base Memory.
 * @param _size Size of the memory, in bytes.
 */
#define PBL_NVMEM_STM32_ROMEM_DEFINE(sym, _name, _base, _size)                                   \
  PBL_DEVICE_STATE_DEFINE(sym);                                                                  \
  PBL_DEVICE_INSTANCE(sym, struct pbl_nvmem_stm32_romem, struct pbl_nvmem, nvmem) {              \
    .nvmem =                                                                                     \
        PBL_NVMEM_INIT(sym, _name, nullptr, nullptr, &pbl_nvmem_stm32_romem_ops, _size, 1U, 1U), \
    .base = (_base),                                                                             \
  }

/** @} */
