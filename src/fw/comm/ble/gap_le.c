/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "gap_le.h"

#include "comm/bt_lock.h"

#include "gap_le_advert.h"
#include "gap_le_connection.h"
#include "gap_le_connect.h"
#include "gap_le_scan.h"
#include "gap_le_slave_discovery.h"
#include "kernel_le_client/kernel_le_client.h"

void gap_le_init(void) {
  bt_lock();
  {
    gap_le_connection_init();
    gap_le_scan_init();
    gap_le_advert_init();
    gap_le_slave_discovery_init();
    // Depends on gap_le_advert:
    gap_le_connect_init();

    kernel_le_client_init();
  }
  bt_unlock();
}

void gap_le_deinit(void) {
  bt_lock();
  {
    kernel_le_client_deinit();

    gap_le_connect_deinit();
    gap_le_slave_discovery_deinit();
    gap_le_advert_deinit();
    gap_le_scan_deinit();
    gap_le_connection_deinit();
  }
  bt_unlock();
}
