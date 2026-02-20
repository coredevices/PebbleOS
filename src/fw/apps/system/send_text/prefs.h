/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/attributes.h"
#include "util/uuid.h"

typedef struct PACKED {
  Uuid contact_uuid;
  Uuid address_uuid;
  bool is_fav;
} SerializedSendTextContact;

typedef struct PACKED {
  uint8_t num_contacts;
  SerializedSendTextContact contacts[];
} SerializedSendTextPrefs;
