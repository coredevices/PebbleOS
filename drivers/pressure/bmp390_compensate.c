/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>

#include <pbl/drivers/pressure/bmp390.h>

static uint16_t prv_u16(const uint8_t *p) {
  return (uint16_t)((p[1] << 8) | p[0]);
}

void pbl_bmp390_parse_calib(const uint8_t raw[21], struct pbl_bmp390_calib *calib) {
  calib->par_t1 = prv_u16(&raw[0]);
  calib->par_t2 = prv_u16(&raw[2]);
  calib->par_t3 = (int8_t)raw[4];
  calib->par_p1 = (int16_t)prv_u16(&raw[5]);
  calib->par_p2 = (int16_t)prv_u16(&raw[7]);
  calib->par_p3 = (int8_t)raw[9];
  calib->par_p4 = (int8_t)raw[10];
  calib->par_p5 = prv_u16(&raw[11]);
  calib->par_p6 = prv_u16(&raw[13]);
  calib->par_p7 = (int8_t)raw[15];
  calib->par_p8 = (int8_t)raw[16];
  calib->par_p9 = (int16_t)prv_u16(&raw[17]);
  calib->par_p10 = (int8_t)raw[19];
  calib->par_p11 = (int8_t)raw[20];
}

// Datasheet compensation (BMP390 datasheet, section 8.4 to 8.6) in single precision, which the
// FPU runs. The terms stay far below the 2^24 integer limit where it matters (raw_temp - t1).
void pbl_bmp390_compensate(const struct pbl_bmp390_calib *c, uint32_t raw_press, uint32_t raw_temp,
                           uint32_t *pa, int32_t *millicelsius) {
  const float t2 = c->par_t2 / 1073741824.0f;
  const float t3 = c->par_t3 / 281474976710656.0f;
  const float p1 = (c->par_p1 - 16384) / 1048576.0f;
  const float p2 = (c->par_p2 - 16384) / 536870912.0f;
  const float p3 = c->par_p3 / 4294967296.0f;
  const float p4 = c->par_p4 / 137438953472.0f;
  const float p5 = c->par_p5 * 8.0f;
  const float p6 = c->par_p6 / 64.0f;
  const float p7 = c->par_p7 / 256.0f;
  const float p8 = c->par_p8 / 32768.0f;
  const float p9 = c->par_p9 / 281474976710656.0f;
  const float p10 = c->par_p10 / 281474976710656.0f;
  const float p11 = c->par_p11 / 36893488147419103232.0f;
  const float d = (float)((int32_t)raw_temp - (int32_t)(c->par_t1 * 256U));
  const float t = d * t2 + d * d * t3;
  const float up = (float)raw_press;
  const float offset = p5 + t * (p6 + t * (p7 + t * p8));
  const float sensitivity = p1 + t * (p2 + t * (p3 + t * p4));
  const float p = offset + up * sensitivity + up * up * (p9 + t * p10) + up * up * up * p11;

  *millicelsius = (int32_t)(t * 1000.0f + (t < 0.0f ? -0.5f : 0.5f));
  *pa = p > 0.0f ? (uint32_t)(p + 0.5f) : 0U;
}
