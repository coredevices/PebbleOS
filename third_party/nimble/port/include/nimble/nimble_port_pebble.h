/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/kernel/thread.h>

/**
 * @brief Start the threads that run NimBLE.
 *
 * The host thread runs the default event queue; with the controller in the
 * image, a second thread runs its link layer. Call once, after
 * nimble_port_init() and after setting up ble_hs_cfg and the GATT services:
 * with BLE_HS_AUTO_START, the host starts as soon as its thread runs.
 */
void nimble_port_pebble_init(void);

#ifdef CONFIG_NIMBLE_STORE_CONFIG
/**
 * @brief Install NimBLE's config store, for the keys of bonded peers and their CCCDs.
 *
 * The store keeps them in RAM and, with a backend that persists (NIMBLE_STORE_BACKEND), loads
 * here what it kept. Call before nimble_port_pebble_init().
 */
void ble_store_config_init(void);
#endif

/** @brief The host thread. */
struct pbl_thread *nimble_port_pebble_host_thread(void);

#if NIMBLE_CFG_CONTROLLER
/** @brief The link layer thread. */
struct pbl_thread *nimble_port_pebble_ll_thread(void);
#endif

#if defined(CONFIG_NIMBLE_HCI_SF32LB) || defined(CONFIG_NIMBLE_HCI_UART)
/** @brief The HCI transport's thread, created by nimble_port_init(). */
struct pbl_thread *nimble_port_pebble_hci_thread(void);
#endif
