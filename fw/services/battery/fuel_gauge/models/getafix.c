/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

// Fitted to a bench pulse test, with R0 lowered from the fitted 4.37 ohm to
// match watch charge logs.

#include <pbl/fuel_gauge/fuel_gauge.h>

static const struct pbl_fuel_gauge_ocv_point s_ocv[] = {
  {0.000f, 3.1131f}, {0.010f, 3.3787f}, {0.020f, 3.4703f}, {0.030f, 3.5172f}, {0.050f, 3.6405f},
  {0.075f, 3.6723f}, {0.100f, 3.6830f}, {0.150f, 3.7017f}, {0.200f, 3.7344f}, {0.250f, 3.7574f},
  {0.300f, 3.7799f}, {0.350f, 3.8028f}, {0.400f, 3.8249f}, {0.450f, 3.8497f}, {0.500f, 3.8787f},
  {0.550f, 3.9120f}, {0.600f, 3.9515f}, {0.650f, 4.0029f}, {0.700f, 4.0610f}, {0.750f, 4.1158f},
  {0.800f, 4.1707f}, {0.850f, 4.2267f}, {0.900f, 4.2844f}, {0.950f, 4.3388f}, {1.000f, 4.3941f},
};

const struct pbl_fuel_gauge_model battery_model = {
  .name = "getafix",
  .capacity_ah = 0.07086f,
  .ocv = s_ocv,
  .ocv_count = sizeof(s_ocv) / sizeof(s_ocv[0]),
  .r0 = 3.0f,
  .r1 = 1.8569f,
  .tau1 = 64.9f,
  .r_temp_coeff = 0.0300f,
};
