/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "services/normal/timeline/attribute.h"
#include "services/normal/timeline/attribute_private.h"
#include "util/attributes.h"

#define APP_GLANCE_DB_CURRENT_VERSION (1)

//! This number is reduced for unit tests to avoid creating large glance payloads in the unit tests
#if UNITTEST
#define APP_GLANCE_DB_MAX_SLICES_PER_GLANCE (2)
#else
#define APP_GLANCE_DB_MAX_SLICES_PER_GLANCE (8)
#endif

#define APP_GLANCE_DB_MAX_NUM_APP_GLANCES (50)

typedef struct PACKED SerializedAppGlanceHeader {
  uint8_t version;
  uint32_t creation_time;
  uint8_t data[]; // Serialized slices
} SerializedAppGlanceHeader;

typedef struct PACKED SerializedAppGlanceSliceHeader {
  uint16_t total_size;
  uint8_t type;
  uint8_t num_attributes;
  uint8_t data[]; // Serialized attributes
} SerializedAppGlanceSliceHeader;

//! The minimum size of an AppGlanceSliceType_IconAndSubtitle slice is the size of the header plus
//! the expiration_time because the icon and subtitle are optional
#define APP_GLANCE_DB_ICON_AND_SUBTITLE_SLICE_MIN_SIZE \
    (sizeof(SerializedAppGlanceSliceHeader) + sizeof(SerializedAttributeHeader) + sizeof(uint32_t))
//! The maximum size of an AppGlanceSliceType_IconAndSubtitle slice is the size of the header plus
//! the expiration_time, icon resource ID, and subtitle string attributes (+1 added for null char)
#define APP_GLANCE_DB_ICON_AND_SUBTITLE_SLICE_MAX_SIZE \
    (sizeof(SerializedAppGlanceSliceHeader) + (sizeof(SerializedAttributeHeader) * 3) + \
        sizeof(uint32_t) + sizeof(uint32_t) + ATTRIBUTE_APP_GLANCE_SUBTITLE_MAX_LEN + 1)

#define APP_GLANCE_DB_SLICE_MIN_SIZE (APP_GLANCE_DB_ICON_AND_SUBTITLE_SLICE_MIN_SIZE)
#define APP_GLANCE_DB_SLICE_MAX_SIZE (APP_GLANCE_DB_ICON_AND_SUBTITLE_SLICE_MAX_SIZE)
