/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "comm/ble/gap_le_scan.h"

/**
 * @defgroup bluetooth_gap_le_scan Scanning
 * @ingroup bluetooth
 * @brief LE scanning.
 *
 * Not implemented by the NimBLE backend: both calls succeed without scanning and no report is
 * delivered.
 *
 * @code{.c}
 * // Active scan, duplicates filtered, 50% duty cycle.
 * if (pbl_bt_start_le_scan(true, false, true, 100, 50)) {
 *   // Reports arrive through pbl_bt_cb_le_scan_handle_report().
 * }
 * // ...
 * pbl_bt_stop_le_scan();
 * @endcode
 * @{
 */

/**
 * @brief Start scanning.
 *
 * @param active_scan true to send scan requests, false for a passive scan.
 * @param use_white_list_filter true to only report devices in the filter accept list.
 * @param filter_dups true to filter out duplicate reports.
 * @param scan_interval_ms Scan interval in milliseconds.
 * @param scan_window_ms Scan window in milliseconds, at most @p scan_interval_ms.
 * @return true on success.
 */
bool pbl_bt_start_le_scan(bool active_scan, bool use_white_list_filter, bool filter_dups,
                          uint16_t scan_interval_ms, uint16_t scan_window_ms);

/**
 * @brief Stop scanning.
 *
 * @return true on success.
 */
bool pbl_bt_stop_le_scan(void);

/**
 * @brief Called for each advertising report received while scanning.
 *
 * @param data The report, followed by its advertising and scan response data.
 * @param length Total length of @p data in bytes.
 */
extern void pbl_bt_cb_le_scan_handle_report(const GAPLERawAdReport *data, int length);

/** @} */
