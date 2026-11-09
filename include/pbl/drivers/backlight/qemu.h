/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/backlight.h>

/**
 * @defgroup drivers_backlight_qemu QEMU
 * @ingroup drivers_backlight
 * @brief QEMU backlight, driving the brightness (or RGB, with @c CONFIG_BACKLIGHT_QEMU_COLOR)
 * registers of the QEMU display.
 * @{
 */

/** @brief QEMU backlight. */
struct pbl_backlight_qemu {
  /** Backlight. */
  struct pbl_backlight bl;
  /** Register base address of the display. */
  uintptr_t base;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_backlight_ops pbl_backlight_qemu_ops;
/** @endcond */

/**
 * @brief Define the backlight of a QEMU display.
 *
 * @param sym Symbol of the backlight.
 * @param _name Name of the display.
 * @param _base Register base address of the display.
 */
#define PBL_BACKLIGHT_QEMU_DEFINE(sym, _name, _base)                                     \
  PBL_BACKLIGHT_STATE_DEFINE(sym);                                                       \
  PBL_DEVICE_INSTANCE(sym, struct pbl_backlight_qemu, struct pbl_backlight, bl) {        \
    .bl = PBL_BACKLIGHT_INIT(sym, _name "_backlight", nullptr, &pbl_backlight_qemu_ops), \
    .base = (_base),                                                                     \
  }

/** @} */
