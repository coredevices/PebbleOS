/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "fake_events.h"
#include "fake_pbl_malloc.h"
#include "kernel/events.h"

//! Strong override of the WEAK fake_events.c implementation, for tests that
//! exercise the GATT client service-change event (which carries a heap-allocated
//! info pointer that must be freed).
//!
//! The default implementation reads the discriminant from gatt_client.subtype,
//! matching the firmware. That works on-target, but on a 64-bit unit-test host
//! uintptr_t is wider than on-target, so the overlapping gatt_client and
//! gatt_client_service union members place their subtype fields at different
//! offsets and neither member alone reliably identifies the variant. The only
//! robust signal on the host is the info pointer itself: a real service-change
//! event holds a live tracked allocation there, whereas the other gatt_client
//! variants leave that slot NULL or filled with non-pointer data. Free it only
//! when it is a tracked allocation.
//!
//! Include this header in the test's main translation unit (the one that pulls
//! in fake_pbl_malloc.h) so the override sees the same allocation tracking list.
void **fake_event_get_buffer(PebbleEvent *event) {
  if (event->type != PEBBLE_BLE_GATT_CLIENT_EVENT) {
    return NULL;
  }
  void **info = (void **)(&event->bluetooth.le.gatt_client_service.info);
  if (*info && fake_pbl_malloc_contains(*info)) {
    return info;
  }
  return NULL;
}
