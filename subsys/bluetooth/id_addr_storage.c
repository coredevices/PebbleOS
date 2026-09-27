/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>

#include <pbl/bluetooth/id_addr.h>
#include <pbl/logging/logging.h>
#include <pbl/services/shared_prf_storage/shared_prf_storage.h>
#include <pbl/util/rand32.h>

PBL_LOG_MODULE_DECLARE(bt, CONFIG_BT_LOG_LEVEL);

// Random static: the two most significant bits are set, and the random part
// is neither all zeros nor all ones.
static void prv_generate(struct pbl_bt_addr *addr) {
  bool valid;

  do {
    uint32_t lo = rand32();
    uint32_t hi = rand32();

    addr->octets[0] = lo;
    addr->octets[1] = lo >> 8;
    addr->octets[2] = lo >> 16;
    addr->octets[3] = lo >> 24;
    addr->octets[4] = hi;
    addr->octets[5] = (hi >> 8) | 0xC0U;

    bool all_zeros = true;
    bool all_ones = (addr->octets[5] == 0xFFU);
    for (size_t i = 0U; i < 5U; i++) {
      all_zeros &= (addr->octets[i] == 0x00U);
      all_ones &= (addr->octets[i] == 0xFFU);
    }
    valid = !all_ones && !(all_zeros && addr->octets[5] == 0xC0U);
  } while (!valid);
}

int pbl_bt_id_addr_get(struct pbl_bt_addr *addr, enum pbl_bt_id_addr_type *type) {
  if (!shared_prf_storage_get_local_identity_address(addr)) {
    prv_generate(addr);
    shared_prf_storage_set_local_identity_address(addr);
    PBL_LOG_INFO("Generated identity address " PBL_BT_ADDR_FMT, PBL_BT_ADDR_XPLODE_PTR(addr));
  }

  *type = PBL_BT_ID_ADDR_RANDOM_STATIC;

  return 0;
}
