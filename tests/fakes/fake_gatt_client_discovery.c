/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "comm/ble/gatt_client_discovery.h"

#include "comm/ble/gap_le_connection.h"

#include "kernel/pbl_malloc.h"
#include "util/list.h"

// Forward declaration of the DiscoveryJobQueue structure
// This matches the definition in gatt_client_discovery.c
typedef struct DiscoveryJobQueue {
  ListNode node;
  ATTHandleRange hdl;
} DiscoveryJobQueue;

void gatt_client_discovery_cleanup_by_connection(GAPLEConnection *connection, BTErrno reason) {
  // Clean up discovery jobs to prevent memory leaks
  // Manually walk the list and free each node
  DiscoveryJobQueue *current = connection->discovery_jobs;
  while (current != NULL) {
    DiscoveryJobQueue *next = (DiscoveryJobQueue *)current->node.next;
    kernel_free(current);
    current = next;
  }
  connection->discovery_jobs = NULL;
}

void gatt_client_subscription_cleanup_by_att_handle_range(
    struct GAPLEConnection *connection, ATTHandleRange *range) { }
