/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/board.h>
#include <pbl/drivers/pmic/npm1300.h>
#include <pbl/drivers/remoteproc/sf32lb.h>

#include <devicetree/devices.h>

#ifdef CONFIG_REMOTEPROC_SF32LB
const struct pbl_rproc_sf32lb *const LCPU =
    container_of(&dt_lcpu, const struct pbl_rproc_sf32lb, rproc);
#endif // CONFIG_REMOTEPROC_SF32LB

#ifdef CONFIG_PMIC_NPM1300
const struct pbl_npm1300_charger *const NPM1300_CHARGER = &dt_pmic_charger;
#endif
