/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/drivers/flash.h>

//! Deep power-down around SoC deep sleep. Called with interrupts disabled.
void pbl_flash_sf32lb52_mpi_dpd_enter(const struct pbl_flash_device *dev);
void pbl_flash_sf32lb52_mpi_dpd_exit(const struct pbl_flash_device *dev);
