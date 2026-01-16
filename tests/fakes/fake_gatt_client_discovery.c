/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "comm/ble/gatt_client_discovery.h"

#include "comm/ble/gap_le_connection.h"

#include "kernel/pbl_malloc.h"
#include "utility/list.h"

// Forward declaration of the DiscoveryJobQueue structure
// This matches the definition in gatt_client_discovery.c
typedef struct DiscoveryJobQueue {
  struct ListNode node;
  ATTHandleRange hdl;
} DiscoveryJobQueue;

void gatt_client_discovery_cleanup_by_connection(GAPLEConnection *connection, BTErrno reason) {
  // Clean up discovery jobs to prevent memory leaks
  // This matches the implementation of gatt_client_cleanup_discovery_jobs in production code
  while (connection->discovery_jobs != NULL) {
    DiscoveryJobQueue *node = connection->discovery_jobs;
    list_remove((ListNode *)connection->discovery_jobs,
                (ListNode **)&connection->discovery_jobs, NULL);
    kernel_free(node);
  }
}

void gatt_client_subscription_cleanup_by_att_handle_range(
    struct GAPLEConnection *connection, ATTHandleRange *range) { }
