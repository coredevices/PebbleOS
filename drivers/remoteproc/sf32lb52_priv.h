/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/drivers/remoteproc/sf32lb52.h>

// Load the LCPU ROM patch into its SRAM region and program the patch controller. LPSYS must be
// awake.
void pbl_rproc_sf32lb52__patch_install(const struct pbl_rproc_sf32lb52 *lcpu);
