/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <pbl/drivers/regulator.h>

/**
 * @defgroup drivers_regulator_sf32lb52_pmuc SF32LB52 PMUC regulators
 * @ingroup drivers_regulator
 * @brief @ref drivers_regulator driver for the regulators of the SF32LB52 PMUC.
 *
 * LDO18 supplies the memory in the package through VDD_SIP, on boards that do not supply that
 * pin themselves. Disabled, it can pull its output down: a board whose own supply drives the pin
 * leaves that off.
 * @{
 */

/** @brief Regulators. */
enum pbl_sf32lb52_pmuc_regulator_id {
  /** 1.8 V LDO of the package memory. */
  PBL_SF32LB52_PMUC_LDO18,
};

/** @brief SF32LB52 PMUC regulator. */
struct pbl_sf32lb52_pmuc_regulator {
  /** Regulator. */
  struct pbl_regulator reg;
  /** PMUC base address. */
  uintptr_t base;
  /** Which one. */
  enum pbl_sf32lb52_pmuc_regulator_id id;
  /** Pull the output down while disabled. */
  bool pull_down;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_regulator_ops pbl_sf32lb52_pmuc_regulator_ops;
/** @endcond */

/**
 * @brief Define an SF32LB52 PMUC regulator.
 *
 * @param sym Symbol of the regulator.
 * @param _name Name.
 * @param _base PMUC base address.
 * @param _id Which one, a @ref pbl_sf32lb52_pmuc_regulator_id.
 * @param _pull_down Pull the output down while disabled.
 * @param _always_on Switched on at init and never off.
 */
#define PBL_SF32LB52_PMUC_REGULATOR_DEFINE(sym, _name, _base, _id, _pull_down, _always_on)        \
  PBL_REGULATOR_STATE_DEFINE(sym);                                                                \
  PBL_DEVICE_INSTANCE(sym, struct pbl_sf32lb52_pmuc_regulator, struct pbl_regulator, reg) {       \
    .reg = PBL_REGULATOR_INIT(sym, _name, nullptr, &pbl_sf32lb52_pmuc_regulator_ops, _always_on), \
    .base = (_base), .id = (_id), .pull_down = (_pull_down),                                      \
  }

/** @} */
