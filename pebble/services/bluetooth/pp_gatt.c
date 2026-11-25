/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/bluetooth/responsiveness.h>
#include <pbl/pp/transports/gatt.h>

#include <mfg/mfg_serials.h>

_Static_assert(PBL_PP_GATT_SERIAL_NUMBER_SIZE == MFG_SERIAL_NUMBER_SIZE);

const char *pbl_pp_gatt_cb_get_serial_number(void) {
  return mfg_get_serial_number();
}

void pbl_pp_gatt_cb_set_responsiveness(uint16_t conn_handle, struct pbl_bt_responsiveness_req *req,
                                       enum pbl_bt_response_time_state state,
                                       uint16_t max_period_secs) {
  pbl_bt_responsiveness_request(conn_handle, req, state, max_period_secs);
}
