/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <pbl/drivers/display/sf32lb57_lcdc.h>
#include <pbl/kernel/mmio.h>

static inline uint32_t prv_read(const struct pbl_display_sf32lb57_lcdc *lcdc, uint32_t offset) {
  return pbl_readl(lcdc->base + offset);
}

static inline void prv_write(const struct pbl_display_sf32lb57_lcdc *lcdc, uint32_t offset,
                             uint32_t value) {
  pbl_writel(value, lcdc->base + offset);
}

static inline void prv_modify(const struct pbl_display_sf32lb57_lcdc *lcdc, uint32_t offset,
                              uint32_t clear, uint32_t set) {
  pbl_modifyl(clear, set, lcdc->base + offset);
}

static inline bool prv_rgb565(const struct pbl_display_sf32lb57_lcdc *lcdc) {
  return lcdc->disp.props.format == PBL_DISPLAY_FORMAT_RGB565;
}

// Waits for the transfer in progress
int pbl_display_sf32lb57_lcdc__wait_idle(const struct pbl_display_sf32lb57_lcdc *lcdc);

// Clocks the controller and resets it
int pbl_display_sf32lb57_lcdc__hw_enable(const struct pbl_display_sf32lb57_lcdc *lcdc);

// Waits for the frame being sent and keeps the next one from starting, with the lock held
int pbl_display_sf32lb57_lcdc__bus_lock(const struct pbl_display_sf32lb57_lcdc *lcdc);
