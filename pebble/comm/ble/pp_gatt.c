/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/pp/transports/gatt.h>

#include <comm/ble/gap_le_connection.h>
#include <comm/bt_conn_mgr.h>
#include <comm/bt_lock.h>
#include <mfg/mfg_serials.h>

_Static_assert(PBL_PP_GATT_SERIAL_NUMBER_SIZE == MFG_SERIAL_NUMBER_SIZE);

const char *pbl_pp_gatt_cb_get_serial_number(void) {
  return mfg_get_serial_number();
}

void pbl_pp_gatt_cb_set_responsiveness(uint16_t conn_handle, enum pbl_bt_consumer consumer,
                                       enum pbl_bt_response_time_state state,
                                       uint16_t max_period_secs,
                                       pbl_bt_responsiveness_granted_cb_t granted) {
  bt_lock();
  GAPLEConnection *connection = gap_le_connection_by_conn_handle(conn_handle);
  if (connection != nullptr) {
    conn_mgr_set_ble_conn_response_time_ext(connection, consumer, state, max_period_secs, granted);
  }
  bt_unlock();
}
