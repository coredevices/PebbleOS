/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/remoteproc/sf32lb52.h>

// Load the LCPU ROM patch into its SRAM region and program the patch controller. LPSYS must be
// awake.
void pbl_rproc_sf32lb52__patch_install(const struct pbl_rproc_sf32lb52 *lcpu);

// Blocks the radio calibration programs
struct pbl_rproc_sf32lb52__rf {
  // RF controller: command and calibration memory, then registers
  uintptr_t rfc;
  uintptr_t phy;
  // Sample capture window the TX calibration reads through DMA
  uintptr_t capture;
  uintptr_t mac;
  uintptr_t gpadc;
  uintptr_t dmac;
  uintptr_t lpsys_rcc;
  uintptr_t hpsys_cfg;
  uintptr_t pmuc;
  // LPSYS SRAM the samples are captured to
  uintptr_t dma_buf;
  // LPSYS SRAM the TX power table is written to, SF32LB57 only
  uintptr_t rf_config;
};

// Calibrate the radio. LPSYS must be awake and the LCPU held. Returns the TX power
// configuration word for the LCPU.
uint32_t pbl_rproc_sf32lb52__rf_cal(const struct pbl_rproc_sf32lb52__rf *rf);

// Calibrate the SF32LB57 radio and write the TX power table. LPSYS must be awake and the LCPU
// held. Returns the TX power configuration word for the LCPU.
uint32_t pbl_rproc_sf32lb57__rf_cal(const struct pbl_rproc_sf32lb52__rf *rf);
