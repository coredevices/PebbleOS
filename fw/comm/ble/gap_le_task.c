/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "gap_le_task.h"

#include "system/passert.h"

PebbleTaskBitset gap_le_pebble_task_bit_for_client(GAPLEClient c) {
  switch (c) {
    case GAPLEClientApp:
      return (1 << PebbleTask_App);
    case GAPLEClientKernel:
      return (1 << PebbleTask_KernelMain);
    default:
      WTF;
  }
}
