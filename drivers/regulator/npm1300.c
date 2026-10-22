/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/regulator/npm1300.h>
#include <pbl/kernel/delay.h>
#include <pbl/logging/logging.h>
#include <pbl/util/bits.h>
#include <pbl/util/misc.h>

PBL_LOG_MODULE_DEFINE(driver_regulator_npm1300, CONFIG_DRIVER_REGULATOR_LOG_LEVEL);

// BUCK block: per-rail registers at base + rail * stride
#define NPM1300_BUCK_ENASET(n)          (0x0400U + 2U * (n))
#define NPM1300_BUCK_ENACLR(n)          (0x0401U + 2U * (n))
#define NPM1300_BUCK_NORMVOUT(n)        (0x0408U + 2U * (n))
#define NPM1300_BUCK_SWCTRLSEL          0x040FU
#define NPM1300_BUCK_VOUTSTATUS(n)      (0x0410U + (n))
#define NPM1300_BUCK_STATUS             0x0434U
#define NPM1300_BUCK_STATUS_MODE(n)     PBL_GENMASK(4U * (n) + 1U, 4U * (n))
#define NPM1300_BUCK_STATUS_MODE_PWM(n) (0x2U << (4U * (n)))

// LDSW block
#define NPM1300_LDSW_TASKSET(n)          (0x0800U + 2U * (n))
#define NPM1300_LDSW_TASKCLR(n)          (0x0801U + 2U * (n))
#define NPM1300_LDSW_STATUS              0x0804U
#define NPM1300_LDSW_STATUS_PWRUPLDSW(n) (1U << (2U * (n)))
#define NPM1300_LDSW_STATUS_PWRUPLDO(n)  (1U << (2U * (n) + 1U))
#define NPM1300_LDSW_LDOSEL(n)           (0x0808U + (n))
#define NPM1300_LDSW_VOUTSEL(n)          (0x080CU + (n))

// BUCK on time that clears the anomaly 27 state
#define NPM1300_ANOMALY27_BUCK_ON_TIME_US 1000U

// Both blocks encode 1.0 V + 100 mV * code
#define NPM1300_VOUT_MIN_MV  1000U
#define NPM1300_VOUT_STEP_MV 100U

static const struct pbl_npm1300_regulator *prv_rail(const struct pbl_regulator *reg) {
  return container_of(reg, const struct pbl_npm1300_regulator, reg);
}

static const struct pbl_npm1300 *prv_pmic(const struct pbl_regulator *reg) {
  return pbl_npm1300_from_cell(&reg->dev);
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
static int prv_buck_init_locked(const struct pbl_npm1300 *pmic, unsigned int n, uint8_t vout) {
  uint8_t voutstatus;
  uint8_t swctrlsel;
  uint8_t initial_vout;
  int ret;

  ret = pbl_npm1300_read(pmic, NPM1300_BUCK_VOUTSTATUS(n), &voutstatus);
  if (ret != 0) {
    return ret;
  }

  initial_vout = (vout != voutstatus) ? vout : (vout ^ 1U);
  ret = pbl_npm1300_write(pmic, NPM1300_BUCK_NORMVOUT(n), initial_vout);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_npm1300_read(pmic, NPM1300_BUCK_SWCTRLSEL, &swctrlsel);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_npm1300_write(pmic, NPM1300_BUCK_SWCTRLSEL, swctrlsel | (1U << n));
  if (ret != 0) {
    return ret;
  }

  if (initial_vout == vout) {
    return 0;
  }

  return pbl_npm1300_write(pmic, NPM1300_BUCK_NORMVOUT(n), vout);
}

// Anomaly 27 state is kept as long as the PMIC stays powered, so it may have been entered on an
// earlier boot, e.g. by firmware without the workaround. The BUCK reports forced PWM mode while in
// it, and cycling the BUCK clears it, but only if it is left on for a while. A rail that has to
// stay on cannot be cycled, so the state is only reported.
static int prv_buck_clear_anomaly27(const struct pbl_npm1300 *pmic, unsigned int n,
                                    bool always_on) {
  uint8_t status;
  int ret;

  ret = pbl_npm1300_read(pmic, NPM1300_BUCK_STATUS, &status);
  if (ret != 0) {
    return ret;
  }

  if ((status & NPM1300_BUCK_STATUS_MODE(n)) != NPM1300_BUCK_STATUS_MODE_PWM(n)) {
    return 0;
  }

  if (always_on) {
    PBL_LOG_ERR("BUCK%u anomaly 27 active, cannot be cleared on an always-on rail", n + 1U);
    return 0;
  }

  ret = pbl_npm1300_write(pmic, NPM1300_BUCK_ENASET(n), 1);
  if (ret != 0) {
    return ret;
  }

  pbl_busy_wait_us(NPM1300_ANOMALY27_BUCK_ON_TIME_US);

  ret = pbl_npm1300_write(pmic, NPM1300_BUCK_ENACLR(n), 1);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_npm1300_read(pmic, NPM1300_BUCK_STATUS, &status);
  if (ret != 0) {
    return ret;
  }

  if ((status & NPM1300_BUCK_STATUS_MODE(n)) == NPM1300_BUCK_STATUS_MODE_PWM(n)) {
    PBL_LOG_ERR("BUCK%u anomaly 27 active, could not be cleared", n + 1U);
  } else {
    PBL_LOG_WRN("BUCK%u anomaly 27 active, cleared", n + 1U);
  }

  return 0;
}

static int prv_ldsw_init_locked(const struct pbl_npm1300 *pmic, unsigned int n, bool ldo,
                                uint8_t vout) {
  uint8_t status;
  int ret;

  ret = pbl_npm1300_read(pmic, NPM1300_LDSW_STATUS, &status);
  if (ret != 0) {
    return ret;
  }

  // Already on as an LDO: only adjust the voltage, without a glitch
  if (ldo && (status & NPM1300_LDSW_STATUS_PWRUPLDO(n))) {
    return pbl_npm1300_write(pmic, NPM1300_LDSW_VOUTSEL(n), vout);
  }

  if (status & (NPM1300_LDSW_STATUS_PWRUPLDSW(n) | NPM1300_LDSW_STATUS_PWRUPLDO(n))) {
    ret = pbl_npm1300_write(pmic, NPM1300_LDSW_TASKCLR(n), 1);
    if (ret != 0) {
      return ret;
    }
  }

  if (ldo) {
    ret = pbl_npm1300_write(pmic, NPM1300_LDSW_VOUTSEL(n), vout);
    if (ret != 0) {
      return ret;
    }
  }

  return pbl_npm1300_write(pmic, NPM1300_LDSW_LDOSEL(n), ldo ? 1U : 0U);
}

static int prv_init(const struct pbl_regulator *reg) {
  const struct pbl_npm1300_regulator *rail = prv_rail(reg);
  const struct pbl_npm1300 *pmic = prv_pmic(reg);
  int ret;

  pbl_npm1300_lock(pmic);
  if (prv_is_buck(rail)) {
    ret = prv_buck_init_locked(pmic, prv_index(rail), prv_vout_code(rail->voltage_mv));
    if (ret == 0) {
      ret = prv_buck_clear_anomaly27(pmic, prv_index(rail), reg->always_on);
    }
  } else {
    ret = prv_ldsw_init_locked(pmic, prv_index(rail), rail->ldo,
                               rail->ldo ? prv_vout_code(rail->voltage_mv) : 0U);
  }
  pbl_npm1300_unlock(pmic);

  return ret;
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

  return pbl_npm1300_write(prv_pmic(reg), task, 1);
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
