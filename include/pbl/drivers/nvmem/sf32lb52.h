/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/clk.h>
#include <pbl/drivers/nvmem.h>

/**
 * @defgroup drivers_nvmem_sf32lb52 SF32LB52 eFuse
 * @ingroup drivers_nvmem
 * @brief @ref drivers_nvmem provider of the SF32LB52 eFuse controller. Read-only.
 * @{
 */

/** @brief SF32LB52 eFuse controller. */
struct pbl_nvmem_sf32lb52 {
  /** NVMEM provider. */
  struct pbl_nvmem nvmem;
  /** Registers. */
  uintptr_t base;
  /** Clock. */
  struct pbl_clk clk;
  /** PMUC registers, holding the HPSYS LDO that reads raise. */
  uintptr_t pmuc;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_nvmem_ops pbl_nvmem_sf32lb52_ops;
extern const struct pbl_nvmem pbl_nvmem_sf32lb52_efuse;
/** @endcond */

/**
 * @brief Define the eFuse.
 *
 * @param sym Symbol.
 * @param _name Device name.
 * @param _base Registers.
 * @param _clk Clock, from PBL_CLK().
 * @param _pmuc PMUC registers.
 */
#define PBL_NVMEM_SF32LB52_DEFINE(sym, _name, _base, _clk, _pmuc)                                  \
  PBL_DEVICE_STATE_DEFINE(sym);                                                                    \
  PBL_DEVICE_INSTANCE(sym, struct pbl_nvmem_sf32lb52, struct pbl_nvmem, nvmem) {                   \
    .nvmem =                                                                                       \
        PBL_NVMEM_INIT(sym, _name, nullptr, nullptr, &pbl_nvmem_sf32lb52_ops, SF32LB52_EFUSE_SIZE, \
                       SF32LB52_EFUSE_WORD_SIZE, SF32LB52_EFUSE_WORD_SIZE),                        \
    .base = (_base), .clk = _clk, .pmuc = (_pmuc),                                                 \
  }

/** eFuse size: four 32-byte banks. */
#define SF32LB52_EFUSE_SIZE 128U
/** eFuse read size and alignment. */
#define SF32LB52_EFUSE_WORD_SIZE 4U

/** eFuse. */
#define SF32LB52_EFUSE (&pbl_nvmem_sf32lb52_efuse)

/** @} */
