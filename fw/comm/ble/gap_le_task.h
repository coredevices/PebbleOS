/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/pebble_tasks.h"

typedef enum GAPLEClient {
  GAPLEClientKernel,
  GAPLEClientApp,

  GAPLEClientNum
} GAPLEClient;

typedef uint8_t GAPLEClientBitset;

//! Converts from GAPLEClient enum to PebbleTaskBitset
PebbleTaskBitset gap_le_pebble_task_bit_for_client(GAPLEClient);
