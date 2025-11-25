/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "comm/ble/gap_le_connection.h"
#include "comm/ble/gap_le_device_name.h"
#include "comm/bt_lock.h"

#include "services/common/bluetooth/bluetooth_persistent_storage.h"
#include "services/common/bluetooth/local_addr.h"
#include "kernel/event_loop.h"
#include "kernel/pbl_malloc.h"
#include "system/logging.h"

#include <bluetooth/bonding_sync.h>
#include <bluetooth/bluetooth_types.h>
#include <bluetooth/sm_types.h>

typedef struct {
  BTBondingID bonding_id;
  BTDeviceAddress addr;
  bool is_gateway;
} CreateBondingContext;

static void prv_finalize_create_bonding(BTBondingID bonding_id,
                                        const BTDeviceAddress *addr,
                                        bool is_gateway) {
  bt_lock();
  GAPLEConnection *connection = gap_le_connection_by_addr(addr);
  if (connection) {
    connection->bonding_id = bonding_id;
    connection->is_gateway = is_gateway;

    if (!connection->is_gateway) {
      PBL_LOG(LOG_LEVEL_DEBUG, "New bonding is not gateway?");
    }

    gap_le_device_name_request(&connection->device);
  } else {
    PBL_LOG(LOG_LEVEL_ERROR, "Couldn't find connection for bonding!");
  }
  bt_unlock();
}

static void prv_finalize_create_bonding_cb(void *data) {
  CreateBondingContext *context = data;
  prv_finalize_create_bonding(context->bonding_id, &context->addr, context->is_gateway);
  kernel_free(context);
}

void bt_driver_cb_handle_create_bonding(const BleBonding *bonding,
                                        const BTDeviceAddress *addr) {
#if !defined(PLATFORM_TINTIN)
  PBL_LOG(LOG_LEVEL_INFO, "Creating new bonding for "BT_DEVICE_ADDRESS_FMT,
          BT_DEVICE_ADDRESS_XPLODE(bonding->pairing_info.identity.address));
#endif
  const bool should_pin_address = bonding->should_pin_address;
  if (should_pin_address) {
    bt_local_addr_pin(&bonding->pinned_address);
  }
  const uint8_t flags = bonding->flags;
  if (flags) {
    PBL_LOG(LOG_LEVEL_INFO, "flags: 0x02%x", flags);
  }
  BTBondingID bonding_id = bt_persistent_storage_store_ble_pairing(&bonding->pairing_info,
                                                                   bonding->is_gateway, NULL,
                                                                   should_pin_address,
                                                                   flags);
  if (bonding_id == BT_BONDING_ID_INVALID) {
    return;
  }

  if (launcher_task_is_current_task()) {
    prv_finalize_create_bonding(bonding_id, addr, bonding->is_gateway);
    return;
  }

  CreateBondingContext *context = kernel_malloc_check(sizeof(*context));

  *context = (CreateBondingContext) {
    .bonding_id = bonding_id,
    .addr = *addr,
    .is_gateway = bonding->is_gateway,
  };
  launcher_task_add_callback(prv_finalize_create_bonding_cb, context);
}
