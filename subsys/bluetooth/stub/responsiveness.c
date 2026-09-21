/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pbl/bluetooth/responsiveness.h"
#include "pbl/bluetooth/gap_le_connect.h"

#include <inttypes.h>

bool pbl_bt_le_connection_parameter_update(const BTDeviceInternal *addr,
                                           const BleConnectionParamsUpdateReq *req) {
  return true;
}
