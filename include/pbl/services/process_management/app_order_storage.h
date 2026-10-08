/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <process_management/app_install_manager.h>
#include <pbl/kernel/compiler.h>

/**
 * @defgroup services_process_management_app_order_storage App menu order storage
 * @ingroup services_process_management
 * @brief Persisted order of the apps in the launcher menu.
 *
 * The order is stored in the PFS file @c lnc_ord with the layout of @ref AppMenuOrderStorage.
 * @{
 */

/** @brief Launcher app order, as stored on flash. */
typedef struct PBL_PACKED AppMenuOrderStorage {
  /** Number of entries in @ref id_list. */
  uint8_t list_length;
  /** Install ids in menu order. */
  AppInstallId id_list[];
} AppMenuOrderStorage;

/**
 * @brief Initialize the app order storage.
 */
void app_order_storage_init(void);

#if UNITTEST
/**
 * @brief Reset the cached "file missing" state.
 *
 * Only built for unit tests.
 */
void app_order_storage_reset_for_tests(void);
#endif

/**
 * @brief Read the stored app order.
 *
 * Must be called from the app task. A corrupt file is removed.
 *
 * @return Order allocated on the app heap, to be freed by the caller with app_free(), or NULL if
 * there is no valid stored order or allocation failed.
 */
AppMenuOrderStorage *app_order_read_order(void);

/**
 * @brief Store an app order given as a list of UUIDs.
 *
 * Must be called from the KernelBG task. UUIDs of apps that are not installed are stored as
 * install id 0.
 *
 * @param uuid_list UUIDs in menu order.
 * @param count Number of entries in @p uuid_list.
 */
void write_uuid_list_to_file(const Uuid *uuid_list, uint8_t count);

/** @} */
