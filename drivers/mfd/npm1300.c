/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/mfd/npm1300.h>
#include <pbl/util/misc.h>

void pbl_npm1300_lock(const struct pbl_npm1300 *pmic) {
  pbl_mutex_lock(&pmic->state->lock, PBL_FOREVER);
}

void pbl_npm1300_unlock(const struct pbl_npm1300 *pmic) {
  pbl_mutex_unlock(&pmic->state->lock);
}

int pbl_npm1300_read(const struct pbl_npm1300 *pmic, uint16_t reg, uint8_t *val) {
  uint8_t regad[2] = {reg >> 8, reg & 0xFF};
  int ret;

  pbl_npm1300_lock(pmic);
  ret = pbl_i2c_write_read(&pmic->i2c, regad, sizeof(regad), val, 1);
  pbl_npm1300_unlock(pmic);

  return ret;
}

int pbl_npm1300_write(const struct pbl_npm1300 *pmic, uint16_t reg, uint8_t val) {
  uint8_t d[3] = {reg >> 8, reg & 0xFF, val};
  int ret;

  pbl_npm1300_lock(pmic);
  ret = pbl_i2c_write(&pmic->i2c, d, sizeof(d));
  pbl_npm1300_unlock(pmic);

  return ret;
}

int pbl_npm1300_init(const struct pbl_device *dev) {
  const struct pbl_npm1300 *pmic = container_of(dev, const struct pbl_npm1300, dev);

  pbl_mutex_init(&pmic->state->lock);

  return (pbl_device_init_children(dev) == 0) ? 0 : -EIO;
}
