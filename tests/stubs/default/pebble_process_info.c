/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/compiler.h>

#include <process_management/pebble_process_info.h>

PBL_WEAK int version_compare(Version a, Version b) {
  return 0;
}
