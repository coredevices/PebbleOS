/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/fuel_gauge/fuel_gauge.h>

// Placeholder until the cell is characterized with tools/fuel_gauge: OCV from
// the voltage-curve discharge table, capacity from the 1C charge current,
// resistances typical of small Li-po cells.

static const struct pbl_fuel_gauge_ocv_point s_ocv[] = {
  {0.00f, 3.300f}, {0.02f, 3.490f}, {0.05f, 3.615f}, {0.10f, 3.655f}, {0.20f, 3.700f},
  {0.30f, 3.735f}, {0.40f, 3.760f}, {0.50f, 3.800f}, {0.60f, 3.855f}, {0.70f, 3.935f},
  {0.80f, 4.025f}, {0.90f, 4.120f}, {1.00f, 4.230f},
};

const struct pbl_fuel_gauge_model battery_model = {
  .name = "asterix-placeholder",
  .capacity_ah = 0.128f,
  .ocv = s_ocv,
  .ocv_count = sizeof(s_ocv) / sizeof(s_ocv[0]),
  .r0 = 0.35f,
  .r1 = 0.15f,
  .tau1 = 100.0f,
  .r_temp_coeff = 0.03f,
};
