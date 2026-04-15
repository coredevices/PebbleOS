/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/uuid.h"
#include "system/status_codes.h"

/* persist_map is an intermediate mapping until app install ids are reimplemented.
 * This is an id, uuid record based solution with the id as a positive native int.
 */

int persist_map_get_size();

int persist_map_add_uuid(const Uuid *uuid);

int persist_map_get_id(const Uuid *uuid);

int persist_map_auto_id(const Uuid *uuid);

int persist_map_get_uuid(int id, Uuid *uuid);

status_t persist_map_init();

//! Dump the persist map to LOG_LEVEL_INFO
void persist_map_dump(void);
