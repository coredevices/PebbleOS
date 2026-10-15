/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>

#include <pbl/kernel/compiler.h>

PBL_WEAK void gap_le_advert_handle_connect_as_slave(void) {
}

PBL_WEAK void gap_le_advert_handle_disconnect_as_slave(void) {
}

PBL_WEAK int8_t gap_le_advert_get_tx_power(void) {
  return -55;
}
