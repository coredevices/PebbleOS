/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "services/common/legacy/registry_common.h"

#include <stdint.h>

//! Initialize the factory settings registry
//!
//! This must be called before using the factory registry.
void factory_registry_init();

//! Add a record to the factory settings registry
//!
//! @return 0 on success, -1 if factory settings registry is full or if key_length or value_length is
//! greater than MAX_KEY_SIZE_BYTES or MAX_VALUE_SIZE_BYTES, respectively
int factory_registry_add(const char* key, const uint8_t key_length, const uint8_t* uuid, const uint8_t description,
        const uint8_t* value, uint8_t value_length);

//! Get a record from the factory settings registry
//!
//! @return Record if found, NULL if not found
Record* factory_registry_get(const char* key, const uint8_t key_length, const uint8_t* uuid);

//! Remove a record from the factory settings registry
//!
//! @return 0 on sucess, -1 if record not found
int factory_registry_remove(const char* key, const uint8_t key_length, const uint8_t* uuid);

//!  Write the factory settings registry to flash
void factory_registry_write_to_flash(void);

//!  Remove all records from the factory settings registry that match `uuid`
void factory_registry_remove_all(const uint8_t* uuid);

