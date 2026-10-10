/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/pmic/npm1300.h>
#include <pbl/util/misc.h>

// BUCK block: per-rail registers at base + rail * stride
#define NPM1300_BUCK_ENASET(n)     (0x0400U + 2U * (n))
#define NPM1300_BUCK_ENACLR(n)     (0x0401U + 2U * (n))
#define NPM1300_BUCK_NORMVOUT(n)   (0x0408U + 2U * (n))
#define NPM1300_BUCK_SWCTRLSEL     0x040FU
#define NPM1300_BUCK_VOUTSTATUS(n) (0x0410U + (n))

// LDSW block
#define NPM1300_LDSW_TASKSET(n)          (0x0800U + 2U * (n))
#define NPM1300_LDSW_TASKCLR(n)          (0x0801U + 2U * (n))
#define NPM1300_LDSW_STATUS              0x0804U
#define NPM1300_LDSW_STATUS_PWRUPLDSW(n) (1U << (2U * (n)))
#define NPM1300_LDSW_STATUS_PWRUPLDO(n)  (1U << (2U * (n) + 1U))
#define NPM1300_LDSW_LDOSEL(n)           (0x0808U + (n))
#define NPM1300_LDSW_VOUTSEL(n)          (0x080CU + (n))

// Both blocks encode 1.0 V + 100 mV * code
#define NPM1300_VOUT_MIN_MV  1000U
#define NPM1300_VOUT_STEP_MV 100U

static const struct pbl_npm1300_regulator *prv_rail(const struct pbl_regulator *reg) {
  return container_of(reg, const struct pbl_npm1300_regulator, reg);
}

static const struct pbl_npm1300 *prv_pmic(const struct pbl_regulator *reg) {
  return container_of(reg->dev.parent, const struct pbl_npm1300, dev);
}

static bool prv_is_buck(const struct pbl_npm1300_regulator *rail) {
  return (rail->rail == PBL_NPM1300_BUCK1) || (rail->rail == PBL_NPM1300_BUCK2);
}

static unsigned int prv_index(const struct pbl_npm1300_regulator *rail) {
  return prv_is_buck(rail) ? (rail->rail - PBL_NPM1300_BUCK1) : (rail->rail - PBL_NPM1300_LDSW1);
}

static uint8_t prv_vout_code(uint16_t mv) {
  return (mv - NPM1300_VOUT_MIN_MV) / NPM1300_VOUT_STEP_MV;
}

// Anomaly 27: when switching a BUCK to SW control, if its NORMVOUT equals the VSET pin value
// (VOUTSTATUS), quiescent current increases by 1 mA. Program a different value first, switch to
// SW control, then set the desired one.
static bool prv_buck_init(const struct pbl_npm1300 *pmic, unsigned int n, uint8_t vout) {
  uint8_t voutstatus;
  uint8_t swctrlsel;

  pbl_npm1300_lock(pmic);

  bool ok = pbl_npm1300_read(pmic, NPM1300_BUCK_VOUTSTATUS(n), &voutstatus);
  uint8_t initial_vout = (vout != voutstatus) ? vout : (vout ^ 1U);
  ok = ok && pbl_npm1300_write(pmic, NPM1300_BUCK_NORMVOUT(n), initial_vout);
  ok = ok && pbl_npm1300_read(pmic, NPM1300_BUCK_SWCTRLSEL, &swctrlsel);
  ok = ok && pbl_npm1300_write(pmic, NPM1300_BUCK_SWCTRLSEL, swctrlsel | (1U << n));
  if (ok && (initial_vout != vout)) {
    ok = pbl_npm1300_write(pmic, NPM1300_BUCK_NORMVOUT(n), vout);
  }

  pbl_npm1300_unlock(pmic);

  return ok;
}

static bool prv_ldsw_init(const struct pbl_npm1300 *pmic, unsigned int n, bool ldo, uint8_t vout) {
  uint8_t status;

  pbl_npm1300_lock(pmic);

  bool ok = pbl_npm1300_read(pmic, NPM1300_LDSW_STATUS, &status);
  if (ok && ldo && (status & NPM1300_LDSW_STATUS_PWRUPLDO(n))) {
    // Already on as an LDO: only adjust the voltage, without a glitch
    ok = pbl_npm1300_write(pmic, NPM1300_LDSW_VOUTSEL(n), vout);
  } else if (ok) {
    if (status & (NPM1300_LDSW_STATUS_PWRUPLDSW(n) | NPM1300_LDSW_STATUS_PWRUPLDO(n))) {
      ok = pbl_npm1300_write(pmic, NPM1300_LDSW_TASKCLR(n), 1);
    }
    if (ldo) {
      ok = ok && pbl_npm1300_write(pmic, NPM1300_LDSW_VOUTSEL(n), vout);
    }
    ok = ok && pbl_npm1300_write(pmic, NPM1300_LDSW_LDOSEL(n), ldo ? 1U : 0U);
  }

  pbl_npm1300_unlock(pmic);

  return ok;
}

static int prv_init(const struct pbl_regulator *reg) {
  const struct pbl_npm1300_regulator *rail = prv_rail(reg);
  bool ok;

  if (prv_is_buck(rail)) {
    ok = prv_buck_init(prv_pmic(reg), prv_index(rail), prv_vout_code(rail->voltage_mv));
  } else {
    ok = prv_ldsw_init(prv_pmic(reg), prv_index(rail), rail->ldo,
                       rail->ldo ? prv_vout_code(rail->voltage_mv) : 0U);
  }

  return ok ? 0 : -EIO;
}

static int prv_set(const struct pbl_regulator *reg, bool on) {
  const struct pbl_npm1300_regulator *rail = prv_rail(reg);
  unsigned int n = prv_index(rail);
  uint16_t task;

  if (prv_is_buck(rail)) {
    task = on ? NPM1300_BUCK_ENASET(n) : NPM1300_BUCK_ENACLR(n);
  } else {
    task = on ? NPM1300_LDSW_TASKSET(n) : NPM1300_LDSW_TASKCLR(n);
  }

  return pbl_npm1300_write(prv_pmic(reg), task, 1) ? 0 : -EIO;
}

static int prv_enable(const struct pbl_regulator *reg) {
  return prv_set(reg, true);
}

static int prv_disable(const struct pbl_regulator *reg) {
  return prv_set(reg, false);
}

const struct pbl_regulator_ops pbl_npm1300_regulator_ops = {
  .init = prv_init,
  .enable = prv_enable,
  .disable = prv_disable,
};
