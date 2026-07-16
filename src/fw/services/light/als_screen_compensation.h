/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

//! Under-display ALS compensation.
//!
//! On boards where the ambient light sensor sits behind the display, the pixels
//! in front of the photodiode modulate how much ambient light reaches it (black
//! pixels block ambient light, white/clear pixels pass it). These helpers
//! correct a raw reading by the luminance of the on-screen sensor region.

struct GBitmap;

//! Pure correction: corrected = raw * gain(L), where gain is the inverse of the
//! front-pixel luminance, gain(L) = 256 / lum_q8, clamped to [1.0x, black_scale].
//! (Transmittance tracks luminance, so a near-black region needs a large gain.)
//! @param raw_level        raw ALS reading in sensor units
//! @param avg_luminance_q8 average front-pixel luminance, Q8 (0=black..256=white)
//! @param black_scale_q8   max gain (region ~black), Q8 (256 == 1.0x disables it)
//! @return corrected reading, clamped to AMBIENT_LIGHT_LEVEL_MAX. No graphics deps.
uint32_t als_compensation_apply(uint32_t raw_level, uint16_t avg_luminance_q8,
                                uint16_t black_scale_q8);

//! Average front-pixel luminance over a framebuffer region, Q8 (0..256).
//! Clamps the region to the bitmap bounds and each row's valid pixel range.
//! Returns 256 (no-op gain) for an empty/zero-area region.
uint16_t als_compensation_region_luminance(const struct GBitmap *fb, int16_t rx,
                                           int16_t ry, int16_t rw, int16_t rh);

//! Sample the live system framebuffer over the board's sensor region and return
//! the average front-pixel luminance, Q8 (0..256). KernelMain-only (reads the
//! compositor's system framebuffer). Returns 256 for an unmeasured region.
uint16_t als_compensation_sample_luminance(void);

//! Correct a raw ALS reading using the live sensor-region luminance and the
//! configured black-scale clamp. Equivalent to als_compensation_apply(
//! raw, als_compensation_sample_luminance(), CONFIG_ALS_BLACK_SCALE_Q8).
//! Reads the framebuffer (see above).
uint32_t als_compensation_correct(uint32_t raw_level);
