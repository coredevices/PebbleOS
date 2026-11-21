/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/display.h>
#include <pbl/util/size.h>

/**
 * @defgroup drivers_display_qemu QEMU display
 * @ingroup drivers_display
 * @brief @ref drivers_display driver for the QEMU framebuffer display peripheral.
 *
 * Rows are copied to the peripheral's framebuffer and shown at once; updates complete before
 * pbl_display_update() returns. The image is not rotated. The peripheral emulates the display
 * described here: at init it is given the size, the format and, for a round display, the mask of
 * the round panel, and fails to initialize if it does not take them.
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
 * @param ... For a round display, the pixels hidden at each end of every row of its top half.
 */
#define PBL_DISPLAY_QEMU_DEFINE(sym, _name, _base, _fb, _width, _height, _bpp, ...)             \
  static_assert((_bpp) == 8 || (_bpp) == 1, "8 or 1 bits per pixel");                           \
  __VA_OPT__(static const uint8_t sym##_round_mask[] = {__VA_ARGS__};                           \
             static_assert(PBL_ARRAY_SIZE(sym##_round_mask) == (_height) / 2,                   \
                           "one round mask entry per row of the top half");)                    \
  PBL_DISPLAY_STATE_DEFINE(sym);                                                                \
  PBL_DEVICE_INSTANCE(sym, struct pbl_display_qemu, struct pbl_display, disp) {                 \
    .disp =                                                                                     \
        PBL_DISPLAY_INIT(sym, _name, nullptr, &pbl_display_qemu_ops, _width, _height,           \
                         ((_bpp) == 1) ? PBL_DISPLAY_FORMAT_MONO : PBL_DISPLAY_FORMAT_ARGB2222, \
                         false __VA_OPT__(, .round_mask = sym##_round_mask)),                   \
    .base = (_base), .fb = (_fb),                                                               \
  }

/** @} */
