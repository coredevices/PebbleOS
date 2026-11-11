/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/board.h>
#include <pbl/drivers/gpio/sf32lb.h>
#include <pbl/drivers/pmic/npm1300.h>
#include <pbl/drivers/remoteproc/sf32lb52.h>
#include <pbl/kernel/irq.h>

#ifdef CONFIG_HRM
#include <pbl/drivers/hrm/gh3x2x.h>
#endif

#include <devicetree/devices.h>

const uint8_t BOARD_DISPLAY_CORNERS[] = {3, 3, 2, 1};
const uint8_t BOARD_DISPLAY_NUM_CORNERS = ARRAY_LENGTH(BOARD_DISPLAY_CORNERS);

#ifdef CONFIG_REMOTEPROC_SF32LB52
const struct pbl_rproc_sf32lb52 *const LCPU =
    container_of(&dt_lcpu, const struct pbl_rproc_sf32lb52, rproc);
#endif // CONFIG_REMOTEPROC_SF32LB52

#ifdef CONFIG_PMIC_NPM1300
const struct pbl_npm1300_charger *const NPM1300_CHARGER = &dt_pmic_charger;
#endif

#ifdef CONFIG_HRM
static const struct pbl_i2c_dev s_i2c_gh3x2x = PBL_I2C_DEV(&dt_i2c4, 0x14);

static HRMDeviceState s_hrm_state;
static HRMDevice s_hrm = {
  .state = &s_hrm_state,
  .i2c = &s_i2c_gh3x2x,
  .int_irq = PBL_IRQ_SPEC(&dt_gpio1_intc, 44, PBL_IRQ_TYPE_EDGE_FALLING),
  .int_input = PBL_GPIO(&dt_gpio1, 44, 0),
  .reset = PBL_GPIO(&dt_pmic_gpio, 3, PBL_GPIO_PULL_UP),
};

HRMDevice *const HRM = &s_hrm;
#endif
