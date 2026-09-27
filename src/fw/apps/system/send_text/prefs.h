/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "pbl/kernel/compiler.h"
#include "pbl/util/uuid.h"

typedef struct PBL_PACKED {
  Uuid contact_uuid;
  Uuid address_uuid;
  bool is_fav;
} SerializedSendTextContact;

typedef struct PBL_PACKED {
  uint8_t num_contacts;
  SerializedSendTextContact contacts[];
} SerializedSendTextPrefs;
