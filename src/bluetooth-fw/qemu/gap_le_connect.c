/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "bluetooth/gap_le_connect.h"

#include <bluetooth/qemu_hrm_bridge.h>

int bt_driver_gap_le_disconnect(const BTDeviceInternal *peer_address) {
  if (bt_driver_qemu_hrm_bridge_disconnect(peer_address)) {
    return 0;
  }
  return 0;
}
