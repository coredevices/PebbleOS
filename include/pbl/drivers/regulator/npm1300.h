/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/mfd/npm1300.h>
#include <pbl/drivers/regulator.h>

/**
 * @defgroup drivers_regulator_npm1300 nPM1300 regulators
 * @ingroup drivers_regulator
 * @brief @ref drivers_regulator rails of the nPM1300, cells of @ref drivers_mfd_npm1300.
 * @{
 */

/** @brief nPM1300 rails. */
enum pbl_npm1300_rail {
  /** BUCK1. */
  PBL_NPM1300_BUCK1,
  /** BUCK2. */
  PBL_NPM1300_BUCK2,
  /** Load switch / LDO 1. */
  PBL_NPM1300_LDSW1,
  /** Load switch / LDO 2. */
  PBL_NPM1300_LDSW2,
};

/** @brief nPM1300 rail. */
struct pbl_npm1300_regulator {
  /** Regulator, child of the core. */
  struct pbl_regulator reg;
  /** Rail. */
  enum pbl_npm1300_rail rail;
  /** Output voltage in mV, 1000 to 3300 in 100 mV steps; unused by a load switch. */
  uint16_t voltage_mv;
  /** LDSW rails only: run as an LDO rather than a load switch. */
  bool ldo;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_regulator_ops pbl_npm1300_regulator_ops;
/** @endcond */

/**
 * @brief Define a PMIC rail.
 *
 * @param sym Symbol of the rail; the regulator is @c sym.reg.
 * @param _name Name.
 * @param _pmic Core, a struct pbl_npm1300.
 * @param _rail Rail, an @ref pbl_npm1300_rail.
 * @param _voltage_mv Output voltage in mV.
 * @param _ldo LDSW rails only: run as an LDO.
 * @param _always_on Switched on at init and never off.
 */
#define PBL_NPM1300_REGULATOR_DEFINE(sym, _name, _pmic, _rail, _voltage_mv, _ldo, _always_on)     \
  PBL_REGULATOR_STATE_DEFINE(sym);                                                                \
  const struct pbl_npm1300_regulator sym = {                                                      \
    .reg = PBL_REGULATOR_INIT(sym, _name, &(_pmic)->dev, &pbl_npm1300_regulator_ops, _always_on), \
    .rail = (_rail),                                                                              \
    .voltage_mv = (_voltage_mv),                                                                  \
    .ldo = (_ldo),                                                                                \
  };                                                                                              \
  PBL_DEVICE_REGISTER(sym, &sym.reg.dev)

/** @} */
