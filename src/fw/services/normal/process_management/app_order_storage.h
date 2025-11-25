/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "process_management/app_install_manager.h"
#include "util/attributes.h"

typedef struct PACKED AppMenuOrderStorage {
  uint8_t list_length;
  AppInstallId id_list[];
} AppMenuOrderStorage;

void app_order_storage_init(void);

//! Returns an AppMenuOrderStorage struct on the kernel heap
AppMenuOrderStorage *app_order_read_order(void);

//! Writes a list of UUID's to the order file
void write_uuid_list_to_file(const Uuid *uuid_list, uint8_t count);
