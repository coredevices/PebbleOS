/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "gap_le.h"

#include "gap_le_connect.h"
#include "gap_le_connection.h"

#include <pbl/services/bluetooth/advertising.h>

#include <comm/ble/kernel_le_client/kernel_le_client.h>
#include <comm/bt_lock.h>

void gap_le_init(void) {
  bt_lock();
  {
    gap_le_connection_init();
    bt_advertising_init();

    kernel_le_client_init();
  }
  bt_unlock();
}

void gap_le_deinit(void) {
  bt_lock();
  {
    kernel_le_client_deinit();

    gap_le_connect_deinit();
    bt_advertising_deinit();
    gap_le_connection_deinit();
  }
  bt_unlock();
}
