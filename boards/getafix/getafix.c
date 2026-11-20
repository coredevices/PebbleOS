/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/board.h>
#include <pbl/drivers/pmic/npm1300.h>

#include <devicetree/devices.h>

#ifdef CONFIG_PMIC_NPM1300
const struct pbl_npm1300_charger *const NPM1300_CHARGER = &dt_pmic_charger;
#endif
