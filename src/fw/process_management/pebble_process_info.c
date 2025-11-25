/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pebble_process_info.h"

int version_compare(Version a, Version b) {
  if (a.major != b.major) {
    return a.major - b.major;
  }
  return a.minor - b.minor;
}

