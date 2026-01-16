/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "comm/ble/gap_le_connection.h"

// Stub for gatt_client_discovery_cleanup_by_connection
// We make this inline so each test can provide its own minimal implementation
// The production version is in gatt_client_discovery.c and will be linked in when needed
static inline void gatt_client_discovery_cleanup_by_connection(struct GAPLEConnection *connection,
                                                                BTErrno reason) {
  // Stub: minimal implementation
  // The real cleanup is done by gatt_client_cleanup_discovery_jobs which is called
  // after this function in prv_destroy_connection
}

// Note: We do NOT stub gatt_client_cleanup_discovery_jobs
// Let the production version from gatt_client_discovery.c be used instead
