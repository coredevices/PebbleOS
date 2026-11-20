/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <math.h>
#include <stdint.h>

#include <pbl/drivers/pressure/bmp390.h>

#include <clar.h>

// The driver is linked in; only its compensation is tested here
int pbl_i2c_transfer(const struct pbl_i2c_dev *dev, struct pbl_i2c_msg *msgs, uint8_t num_msgs) {
  return -EIO;
}

// Trimming bytes of a BMP390, registers 0x31 to 0x45
static const uint8_t s_raw_calib[21] = {
  0x2c, 0x6b, 0x5e, 0x4a, 0xf9, 0xe9, 0xfb, 0x2b, 0xfd, 0x22, 0xfb,
  0x54, 0x56, 0x40, 0x75, 0x04, 0xf3, 0xae, 0x3e, 0xea, 0xf5,
};

// Floating-point compensation of the BMP390 datasheet, section 8.4/8.5
static void prv_reference(const struct pbl_bmp390_calib *c, uint32_t raw_press, uint32_t raw_temp,
                          double *celsius, double *pa) {
  const double t1 = c->par_t1 * 256.0;
  const double t2 = c->par_t2 / 1073741824.0;
  const double t3 = c->par_t3 / 281474976710656.0;
  const double p1 = (c->par_p1 - 16384) / 1048576.0;
  const double p2 = (c->par_p2 - 16384) / 536870912.0;
  const double p3 = c->par_p3 / 4294967296.0;
  const double p4 = c->par_p4 / 137438953472.0;
  const double p5 = c->par_p5 * 8.0;
  const double p6 = c->par_p6 / 64.0;
  const double p7 = c->par_p7 / 256.0;
  const double p8 = c->par_p8 / 32768.0;
  const double p9 = c->par_p9 / 281474976710656.0;
  const double p10 = c->par_p10 / 281474976710656.0;
  const double p11 = c->par_p11 / 36893488147419103232.0;
  const double up = raw_press;
  const double d = raw_temp - t1;
  const double t = d * t2 + d * d * t3;

  *celsius = t;
  *pa = (p5 + p6 * t + p7 * t * t + p8 * t * t * t) +
        up * (p1 + p2 * t + p3 * t * t + p4 * t * t * t) + up * up * (p9 + p10 * t) +
        up * up * up * p11;
}

void test_bmp390_compensate__parse(void) {
  struct pbl_bmp390_calib c;

  pbl_bmp390_parse_calib(s_raw_calib, &c);

  cl_assert_equal_i(c.par_t1, 0x6b2c);
  cl_assert_equal_i(c.par_t2, 0x4a5e);
  cl_assert_equal_i(c.par_t3, -7);
  cl_assert_equal_i(c.par_p1, (int16_t)0xfbe9);
  cl_assert_equal_i(c.par_p5, 0x5654);
  cl_assert_equal_i(c.par_p9, 0x3eae);
  cl_assert_equal_i(c.par_p10, (int8_t)0xea);
  cl_assert_equal_i(c.par_p11, (int8_t)0xf5);
}

void test_bmp390_compensate__matches_datasheet(void) {
  struct pbl_bmp390_calib c;
  int checked = 0;

  pbl_bmp390_parse_calib(s_raw_calib, &c);

  // Sweep the raw range covering about -20 to 60 C and 300 to 1100 hPa
  for (uint32_t raw_temp = 5750000U; raw_temp <= 10000000U; raw_temp += 250000U) {
    for (uint32_t raw_press = 4000000U; raw_press <= 9000000U; raw_press += 250000U) {
      uint32_t pa_out;
      double celsius;
      double pa;

      pa_out = pbl_bmp390_compensate(&c, raw_press, raw_temp);
      prv_reference(&c, raw_press, raw_temp, &celsius, &pa);

      if (celsius < -40.0 || celsius > 85.0 || pa < 30000.0 || pa > 125000.0) {
        continue;
      }

      // Within rounding of the double-precision reference
      cl_assert(fabs(pa_out - pa) < 1.0);
      checked++;
    }
  }

  cl_assert(checked > 20);
}
