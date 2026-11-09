/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/display.h>
#include <pbl/drivers/panel/ls013b7dd02.h>

/**
 * @defgroup drivers_display_qemu QEMU display
 * @ingroup drivers_display
 * @brief @ref drivers_display driver for the QEMU framebuffer display peripheral.
 *
 * Rows are copied to the peripheral's framebuffer and shown at once; updates complete before
 * pbl_display_update() returns. The image is not rotated. A round display hides the corners of
 * the 260x260 round panel the machine emulates.
 * @{
 */

/** @brief QEMU display. */
struct pbl_display_qemu {
  /** Display. */
  struct pbl_display disp;
  /** Register base address. */
  uintptr_t base;
  /** Framebuffer base address. */
  uintptr_t fb;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_display_ops pbl_display_qemu_ops;
/** @endcond */

/**
 * @brief Define a QEMU display.
 *
 * @param sym Symbol of the display.
 * @param _name Name.
 * @param _base Register base address.
 * @param _fb Framebuffer base address.
 * @param _width Width, in pixels.
 * @param _height Height, in pixels.
 * @param _bpp Bits per pixel: 8 for ARGB2222, 1 for monochrome.
 * @param _round Whether the display is round.
 */
#define PBL_DISPLAY_QEMU_DEFINE(sym, _name, _base, _fb, _width, _height, _bpp, _round) \
  static_assert((_bpp) == 8 || (_bpp) == 1, "8 or 1 bits per pixel");                  \
  PBL_DISPLAY_STATE_DEFINE(sym);                                                       \
  PBL_DEVICE_INSTANCE(sym, struct pbl_display_qemu, struct pbl_display, disp) {        \
    .disp = PBL_DISPLAY_INIT(                                                          \
        sym, _name, &pbl_display_qemu_ops, nullptr, _width, _height,                   \
        ((_bpp) == 1) ? PBL_DISPLAY_FORMAT_MONO : PBL_DISPLAY_FORMAT_ARGB2222, false,  \
        .round_mask = (_round) ? pbl_panel_ls013b7dd02_round_mask : nullptr),          \
    .base = (_base), .fb = (_fb),                                                      \
  }

/** @} */
