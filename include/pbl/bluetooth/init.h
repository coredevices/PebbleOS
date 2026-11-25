/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/bluetooth/dis.h>
#include <pbl/bluetooth/sm_types.h>
#include <pbl/kernel/malloc.h>

/**
 * @defgroup bluetooth Bluetooth
 * @ingroup subsys
 * @brief Bluetooth Low Energy stack interface (@c pbl/bluetooth).
 *
 * The subsystem runs the NimBLE host, in its own task, and adds what an application needs around
 * it: the bond store, the GATT services and clients, pairing confirmation and connection
 * response times. Everything else is done with NimBLE directly: connections are named by their
 * handles, and their events are followed with ble_gap_event_listener_register().
 *
 * A few events are reported through callbacks the application implements, declared @c extern
 * and named @c pbl_bt_cb_*. Unless stated otherwise they run on the NimBLE host task.
 *
 * The NimBLE host reaches its controller through the HCI transport selected with the
 * @c NIMBLE_HCI_* Kconfig choice: the in-image NimBLE link layer (@c NIMBLE_HCI_NRF52), the SiFli
 * LCPU over IPC (@c NIMBLE_HCI_SF32LB), H4 over UART (@c NIMBLE_HCI_UART) or a fake controller
 * (@c NIMBLE_HCI_FAKE). The fake controller, used on QEMU, accepts every HCI command and never
 * raises an event: the host synchronizes and the GATT server runs, but no link is ever established.
 *
 * The watch is a peripheral only. Typical bring-up:
 *
 * @code{.c}
 * pbl_bt_init(); // once, at boot
 *
 * struct pbl_bt_config *config = pbl_zalloc_check(sizeof(*config));
 * dis_get_info(&config->dis_info);
 * int rc = pbl_bt_start(config);
 * pbl_free(config);
 *
 * // ...
 *
 * pbl_bt_stop();
 * @endcode
 * @{
 */

/** @brief Stack configuration passed to pbl_bt_start(). */
struct pbl_bt_config {
  /** The local Identity Resolving Key. */
  struct pbl_bt_sm_key irk;
  /** Values served by the Device Information Service. */
  struct pbl_bt_dis_info dis_info;
};

/**
 * @brief One-time initialization of the Bluetooth stack.
 *
 * Creates the NimBLE host task (and the link layer task when the controller runs in the same
 * image) and installs the bond store. Call once at boot, before any other function of this
 * API.
 */
void pbl_bt_init(void);

/**
 * @brief Start the Bluetooth stack.
 *
 * Registers the GATT services, starts the host and waits for it to synchronize with the
 * controller and set the identity address up. Does nothing if the stack is already running.
 *
 * @param config Stack configuration. Only read during the call.
 * @retval 0 The stack is running.
 * @retval -EBUSY It is starting or stopping.
 * @retval -EIO The GATT services could not be registered.
 * @retval -EADDRNOTAVAIL There is no identity address.
 */
int pbl_bt_start(const struct pbl_bt_config *config);

/**
 * @brief Stop the Bluetooth stack.
 *
 * Blocks until the host has stopped, then resets the GATT server. Bonds are kept. Does nothing if
 * the stack is not running.
 */
void pbl_bt_stop(void);

/**
 * @brief Called each time the host (re-)synchronizes with the controller.
 *
 * Implemented by the firmware. The controller state is lost on a host reset, so this is where
 * advertising data and parameters must be pushed again. Invoked on the NimBLE host task.
 */
extern void pbl_bt_cb_host_synced(void);

/** @} */
