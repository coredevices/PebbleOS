/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "local_addr.h"

#include "comm/bt_lock.h"
#include "system/logging.h"
#include "system/passert.h"

#include <bluetooth/bluetooth_types.h>
#include <bluetooth/id.h>
#include <btutil/bt_device.h>

static uint32_t s_pra_cycling_pause_count;
static BTDeviceAddress s_pinned_addr;
static bool s_cycling_paused_due_to_dependent_bondings;

static void prv_allow_cycling(bool allow_cycling) {
  bt_driver_set_local_address(allow_cycling, allow_cycling ? NULL : &s_pinned_addr);
}

void bt_local_addr_pause_cycling(void) {
  bt_lock();
  {
    if (s_pra_cycling_pause_count == 0) {
      PBL_LOG(LOG_LEVEL_INFO, "Pausing address cycling (pinned_addr="BT_DEVICE_ADDRESS_FMT")",
              BT_DEVICE_ADDRESS_XPLODE(s_pinned_addr));
      prv_allow_cycling(false);
    }
    ++s_pra_cycling_pause_count;
  }
  bt_unlock();
}

void bt_local_addr_resume_cycling(void) {
  bt_lock();
  {
    PBL_ASSERTN(s_pra_cycling_pause_count);
    --s_pra_cycling_pause_count;
    if (s_pra_cycling_pause_count == 0) {
      PBL_LOG(LOG_LEVEL_INFO, "Resuming address cycling (pinned_addr="BT_DEVICE_ADDRESS_FMT")",
              BT_DEVICE_ADDRESS_XPLODE(s_pinned_addr));
      prv_allow_cycling(true);
    }
  }
  bt_unlock();
}

void bt_local_addr_pin(const BTDeviceAddress *addr) {
  // In a previous version of the code, the main FW would not know yet what address would be used
  // for pinning until the BT driver would give the address to pin when a pairing was added.
  // A single, persistent pinned address is now generated up front in bt_local_addr_init().
  // Getting the address back in this call from the BT driver currently only serves as a
  // consistency check.
  // It is possible that the addresses do not match in the following scenario:
  // 1. No bondings that require pinning present. Cycling address 'C' is used.
  // 2. Device A is connected.
  // 3. Become discoverable: cycling is requested to be paused at address 'P' but can't be granted
  //    yet because device A is still connected.
  // 4. Device B connects (using 'C' as connection address)
  // 5. Device B requests pin + pairs => the remote bonding is stored with 'C' as key instead of 'P'
  // 6. We'll print here there's a mismatch.
  // 7. Once Device A & B disconnect, device B won't be able to recognize us because 'P' is used...

  bt_lock();
  bool addresses_match = bt_device_address_equal(addr, &s_pinned_addr);
  bt_unlock();

  PBL_LOG(LOG_LEVEL_INFO,
          "Requested to pin address to "BT_DEVICE_ADDRESS_FMT " match=%u",
          BT_DEVICE_ADDRESS_XPLODE_PTR(addr), addresses_match);
}

void bt_local_addr_handle_bonding_change(BTBondingID bonding, BtPersistBondingOp op) {
  bool has_pinned_ble_pairings = bt_persistent_storage_has_pinned_ble_pairings();
  if (has_pinned_ble_pairings != s_cycling_paused_due_to_dependent_bondings) {
    if (has_pinned_ble_pairings) {
      bt_local_addr_pause_cycling();
    } else {
      bt_local_addr_resume_cycling();
    }
    s_cycling_paused_due_to_dependent_bondings = has_pinned_ble_pairings;
  }
}

void bt_local_addr_init(void) {
  s_pra_cycling_pause_count = 0;
  s_cycling_paused_due_to_dependent_bondings = false;

  // Load pinned address from settings file or generate one if it hasn't happened before:
  if (!bt_persistent_storage_get_ble_pinned_address(&s_pinned_addr)) {
    if (bt_driver_id_generate_private_resolvable_address(&s_pinned_addr)) {
      bt_persistent_storage_set_ble_pinned_address(&s_pinned_addr);
    } else {
      PBL_LOG(LOG_LEVEL_ERROR, "Failed to generate PRA... :(");
    }
  }
  PBL_LOG(LOG_LEVEL_INFO, "Pinned address: " BT_DEVICE_ADDRESS_FMT,
          BT_DEVICE_ADDRESS_XPLODE(s_pinned_addr));

  if (bt_persistent_storage_has_pinned_ble_pairings()) {
    PBL_LOG(LOG_LEVEL_INFO, "Bonding that requires address pinning exists, applying pinned addr!");
    bt_local_addr_pause_cycling();
    s_cycling_paused_due_to_dependent_bondings = true;
  } else {
#if RECOVERY_FW
    PBL_LOG(LOG_LEVEL_INFO, "Pausing address cycling because PRF!");
    bt_local_addr_pause_cycling();
#else
    PBL_LOG(LOG_LEVEL_INFO, "No bondings found that require address pinning!");
    bt_driver_set_local_address(true /* allow_cycling */, NULL);
#endif
  }
}
