/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "pbl/kernel/compiler.h"

#include <pbl/bluetooth/sm_types.h>
#include <pbl/bluetooth/dis.h>

#include <stdbool.h>

/**
 * @defgroup bluetooth Bluetooth
 * @ingroup subsys
 * @brief Bluetooth Low Energy stack interface (@c pbl/bluetooth).
 *
 * The firmware's Bluetooth code (@c fw/comm/ble and @c fw/services/bluetooth) talks to the stack
 * only through the @c pbl_bt_* API declared in these headers. It is implemented in
 * @c subsys/bluetooth on top of the NimBLE host, which runs in its own task.
 *
 * The API has two directions:
 * - Calls into the stack, such as pbl_bt_start(), pbl_bt_advert_advertising_enable() or
 *   pbl_bt_gatt_read().
 * - Callbacks, declared @c extern and named @c pbl_bt_handle_* or @c pbl_bt_cb_*, that the
 *   firmware implements and the stack invokes to report events. Unless stated otherwise they run
 *   on the NimBLE host task without @c bt_lock() held.
 *
 * The NimBLE host reaches its controller through the HCI transport selected with the
 * @c BT_HCI_* Kconfig choice: the in-image NimBLE link layer (@c BT_HCI_NRF52), the SiFli LCPU
 * over IPC (@c BT_HCI_SF32LB52), H4 over UART (@c BT_HCI_UART) or a fake controller
 * (@c BT_HCI_FAKE). The fake controller, used on QEMU, accepts every HCI command and never raises
 * an event: the host synchronizes and the GATT server runs, but no link is ever established.
 *
 * The watch is a peripheral only. Typical bring-up:
 *
 * @code{.c}
 * pbl_bt_init(); // once, at boot
 *
 * struct pbl_bt_config *config = kernel_zalloc_check(sizeof(*config));
 * dis_get_info(&config->dis_info);
 * // Bondings registered before starting are restored by the host.
 * bt_persistent_storage_register_existing_ble_bondings();
 * bool running = pbl_bt_start(config);
 * kernel_free(config);
 *
 * // ...
 *
 * pbl_bt_stop();
 * @endcode
 * @{
 */

/** @brief Stack configuration passed to pbl_bt_start(). */
struct PBL_PACKED pbl_bt_config {
  /**
   * @brief Root keys, indexed by enum pbl_bt_sm_root_key_type.
   *
   * Not used by the NimBLE backend, which reads and generates the root keys through the
   * persistent storage service.
   */
  struct pbl_bt_sm_key root_keys[PBL_BT_SM_ROOT_KEY_TYPE_NUM];
  /** Values served by the Device Information Service. */
  struct pbl_bt_dis_info dis_info;
  /** Identity address. Not used by the NimBLE backend, see pbl_bt_id_addr_get(). */
  struct pbl_bt_addr identity_addr;
  /** Whether the heart rate service may be exposed. Not used by the NimBLE backend. */
  bool is_hrm_supported_and_enabled;
};

/**
 * @brief One-time initialization of the Bluetooth stack.
 *
 * Creates the NimBLE host task (and the link layer task when the controller runs in the same
 * image) and installs the bonding store. Call once at boot, before any other function of this
 * API.
 */
void pbl_bt_init(void);

/**
 * @brief Start the Bluetooth stack.
 *
 * Registers the GATT services, starts the host and waits for it to synchronize with the
 * controller, then sets up the identity address. Does nothing if the stack is already running.
 *
 * @param config Stack configuration. Only read during the call.
 * @return true if the stack is running, false if it failed to start.
 */
bool pbl_bt_start(struct pbl_bt_config *config);

/**
 * @brief Stop the Bluetooth stack.
 *
 * Blocks until the host has stopped, then resets the GATT server and drops the in-memory bonding
 * store. No bonding removal callbacks are issued.
 */
void pbl_bt_stop(void);

/**
 * @brief Power down the controller at boot if Bluetooth is not going to be started.
 *
 * Does nothing in the NimBLE backend.
 */
void pbl_bt_power_down_controller_on_boot(void);

/**
 * @brief Called each time the host (re-)synchronizes with the controller.
 *
 * Implemented by the firmware. The controller state is lost on a host reset, so this is where
 * advertising data and parameters must be pushed again. Invoked on the NimBLE host task.
 */
extern void pbl_bt_handle_host_resynced(void);

/** @} */
