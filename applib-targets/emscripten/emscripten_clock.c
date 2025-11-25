/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "services/common/clock.h"

static bool s_clock_is_24h_style;

bool clock_is_24h_style(void) {
  return s_clock_is_24h_style;
}

void clock_set_24h_style(bool is_24h_style) {
  s_clock_is_24h_style = is_24h_style;
}
