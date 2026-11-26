/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/nvmem.h>

/**
 * @defgroup drivers_nvmem_nrf_ficr Nordic nRF FICR
 * @ingroup drivers_nvmem
 * @brief Read access to the factory information of the Nordic nRF SoCs, as their device ID and
 * the trims of their crystal oscillators.
 * @{
 */

/** @brief Factory information configuration registers. */
struct pbl_nvmem_nrf_ficr {
  /** NVMEM provider. */
  struct pbl_nvmem nvmem;
  /** Registers. */
  uintptr_t base;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_nvmem_ops pbl_nvmem_nrf_ficr_ops;
extern const struct pbl_nvmem pbl_nvmem_nrf_ficr;
/** @endcond */

/**
 * @brief Define the FICR.
 *
 * @param sym Symbol.
 * @param _name Device name.
 * @param _base Registers.
 * @param _size Size of the registers, in bytes.
 */
#define PBL_NVMEM_NRF_FICR_DEFINE(sym, _name, _base, _size)                                        \
  PBL_DEVICE_STATE_DEFINE(sym);                                                                    \
  PBL_DEVICE_INSTANCE(sym, struct pbl_nvmem_nrf_ficr, struct pbl_nvmem, nvmem) {                   \
    .nvmem = PBL_NVMEM_INIT(sym, _name, nullptr, nullptr, &pbl_nvmem_nrf_ficr_ops, _size, 4U, 4U), \
    .base = (_base),                                                                               \
  }

/** The FICR. */
#define NRF_FICR (&pbl_nvmem_nrf_ficr)

/** @} */
