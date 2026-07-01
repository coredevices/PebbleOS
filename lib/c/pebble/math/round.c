/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

// Naive implementation, probably room for optimization...
double round(double d) {
  if (__builtin_floor(d) == d) {
    return d;
  }
  double half;
  if (d < 0.0) {
    if (d > -0.5) {
      return -0.0;
    } else {
      half = -0.5;
    }
  } else {
    half = 0.5;
  }
  return __builtin_floor(d + half);
}
