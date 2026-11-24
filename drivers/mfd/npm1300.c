/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/mfd/npm1300.h>
#include <pbl/kernel/delay.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/sched.h>
#include <pbl/util/bits.h>
#include <pbl/util/byteorder.h>
#include <pbl/util/misc.h>

#define NPM1300_VBUSINSTATUS         0x0207U
#define NPM1300_VBUSINSTATUS_PRESENT PBL_BIT(0)
#define NPM1300_TASKENTERSHIPMODE    0x0B02U

// tPWRDN
#define SHIP_MODE_ENTRY_US 100000U

void pbl_npm1300_lock(const struct pbl_npm1300 *pmic) {
  pbl_mutex_lock(&pmic->state->lock, PBL_FOREVER);
}

void pbl_npm1300_unlock(const struct pbl_npm1300 *pmic) {
  pbl_mutex_unlock(&pmic->state->lock);
}

int pbl_npm1300_read(const struct pbl_npm1300 *pmic, uint16_t reg, uint8_t *val) {
  uint8_t regad[2];
  int ret;

  pbl_put_be16(reg, regad);
  pbl_npm1300_lock(pmic);
  ret = pbl_i2c_write_read(&pmic->i2c, regad, sizeof(regad), val, 1);
  pbl_npm1300_unlock(pmic);

  return ret;
}

int pbl_npm1300_write(const struct pbl_npm1300 *pmic, uint16_t reg, uint8_t val) {
  uint8_t d[3];
  int ret;

  pbl_put_be16(reg, d);
  d[2] = val;
  pbl_npm1300_lock(pmic);
  ret = pbl_i2c_write(&pmic->i2c, d, sizeof(d));
  pbl_npm1300_unlock(pmic);

  return ret;
}

int pbl_npm1300_init(const struct pbl_device *dev) {
  const struct pbl_npm1300 *pmic = PBL_CONTAINER_OF(dev, const struct pbl_npm1300, dev);

  pbl_mutex_init(&pmic->state->lock);

  return (pbl_device_init_children(dev) == 0) ? 0 : -EIO;
}

int pbl_npm1300_power_cut_check(void *user_data) {
  const struct pbl_npm1300 *pmic = user_data;
  uint8_t status;
  int ret;

  if (!pmic->system_power_controller) {
    return -ENOTSUP;
  }

  // Ship mode exits as soon as VBUS is present
  ret = pbl_npm1300_read(pmic, NPM1300_VBUSINSTATUS, &status);
  if (ret != 0) {
    return ret;
  }

  return ((status & NPM1300_VBUSINSTATUS_PRESENT) != 0U) ? -EBUSY : 0;
}

void pbl_npm1300_power_cut(enum pbl_sys_off_mode mode, void *user_data) {
  const struct pbl_npm1300 *pmic = user_data;

  // I2C transfers need the scheduler
  if (pbl_in_isr() || pbl_irq_is_locked() || !pbl_kernel_is_running()) {
    return;
  }

  if (pbl_npm1300_power_cut_check(user_data) != 0) {
    return;
  }

  if (pbl_npm1300_write(pmic, NPM1300_TASKENTERSHIPMODE, 1) != 0) {
    return;
  }

  pbl_busy_wait_us(SHIP_MODE_ENTRY_US);
}

#include PBL_DT_INSTANCES(nordic_npm1300_npm1300)
