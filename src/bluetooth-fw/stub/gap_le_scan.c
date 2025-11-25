/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "bluetooth/gap_le_scan.h"

bool bt_driver_start_le_scan(bool active_scan, bool use_white_list_filter, bool filter_dups,
                            uint16_t scan_interval_ms, uint16_t scan_window_ms) {
  return true;
}


bool bt_driver_stop_le_scan(void) {
  return true;
}
