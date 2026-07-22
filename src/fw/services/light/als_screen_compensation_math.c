/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "services/light/als_screen_compensation.h"

#include <pbl/drivers/ambient_light.h>  // AMBIENT_LIGHT_LEVEL_MAX

#define ALS_COMP_UNITY_Q8 (256u)

uint32_t als_compensation_apply(uint32_t raw_level, uint16_t avg_luminance_q8,
                                uint16_t black_scale_q8) {
  // Inverse-luminance model: the under-display transmittance tracks the pixel
  // luminance, so gain(L) = L_white / L = 256 / lum_q8, clamped to
  // [1.0x, black_scale]. Measured T(L) on getafix fits this within ~4% at every
  // ambient level; a gain that is *linear* in luminance was off by ~5x at the
  // mid-grays. black_scale only bounds the gain where the region is ~fully black.
  uint32_t lum_q8 = avg_luminance_q8;
  if (lum_q8 > ALS_COMP_UNITY_Q8) {
    lum_q8 = ALS_COMP_UNITY_Q8;  // white -> unity
  }
  if (lum_q8 == 0) {
    lum_q8 = 1;  // avoid divide-by-zero; the black clamp below caps the result
  }

  uint32_t gain_q8 = (ALS_COMP_UNITY_Q8 * ALS_COMP_UNITY_Q8) / lum_q8;  // 256/L in Q8
  if (gain_q8 < ALS_COMP_UNITY_Q8) {
    gain_q8 = ALS_COMP_UNITY_Q8;  // never attenuate below 1.0x
  }
  uint32_t black_scale = black_scale_q8;
  if (black_scale < ALS_COMP_UNITY_Q8) {
    black_scale = ALS_COMP_UNITY_Q8;  // black_scale == 1.0x disables compensation
  }
  if (gain_q8 > black_scale) {
    gain_q8 = black_scale;  // clamp the dark end
  }

  // 64-bit intermediate: raw (up to 1<<16) * gain_q8 can exceed 32 bits.
  const uint64_t corrected = ((uint64_t)raw_level * gain_q8) / ALS_COMP_UNITY_Q8;
  if (corrected > AMBIENT_LIGHT_LEVEL_MAX) {
    return AMBIENT_LIGHT_LEVEL_MAX;
  }
  return (uint32_t)corrected;
}
