/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pbl/bluetooth/responsiveness.h"
#include "pbl/bluetooth/gap_le_connect.h"

#include <inttypes.h>

bool pbl_bt_le_connection_parameter_update(const struct pbl_bt_device_internal *addr,
                                           const struct pbl_bt_conn_params_update_req *req) {
  return true;
}
