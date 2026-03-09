/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "bluetooth/bluetooth_types.h"
#include "comm/ble/gap_le_connection.h"
#include "kernel/pbl_malloc.h"
#include "util/list.h"

// Forward declaration of the DiscoveryJobQueue structure
// This MUST match the definition in gatt_client_discovery.c exactly
typedef struct DiscoveryJobQueue {
  ListNode node;
  ATTHandleRange hdl;
} DiscoveryJobQueue;

static inline void gatt_client_discovery_cleanup_by_connection(struct GAPLEConnection *connection,
                                                                BTErrno reason) {
  // Stub implementation: clean up discovery jobs to prevent memory leaks
  // Manually walk the list and free each node
  if (!connection) {
    return;
  }
  DiscoveryJobQueue *current = connection->discovery_jobs;
  while (current != NULL) {
    DiscoveryJobQueue *next = (DiscoveryJobQueue *)current->node.next;
    kernel_free(current);
    current = next;
  }
  connection->discovery_jobs = NULL;
}

// Stub for gatt_client_cleanup_discovery_jobs
// This is needed for tests that don't include gatt_client_discovery.c
static inline void gatt_client_cleanup_discovery_jobs(struct GAPLEConnection *connection) {
  // Just call gatt_client_discovery_cleanup_by_connection to clean up
  gatt_client_discovery_cleanup_by_connection(connection, BTErrnoOK);
}
