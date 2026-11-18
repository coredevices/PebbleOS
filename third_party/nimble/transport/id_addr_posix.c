/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "id_addr_posix_bottom.h"

#include <errno.h>

#include <pbl/bluetooth/id_addr.h>

int pbl_bt_id_addr_get(struct pbl_bt_addr *addr, enum pbl_bt_id_addr_type *type) {
  if (!id_addr_posix_bottom_get(addr->octets)) {
    return -ENOENT;
  }

  *type = PBL_BT_ID_ADDR_RANDOM_STATIC;

  return 0;
}
