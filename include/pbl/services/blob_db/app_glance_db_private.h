/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "pbl/services/timeline/attribute.h"
#include "pbl/services/timeline/attribute_private.h"
#include "pbl/kernel/compiler.h"

/**
 * @defgroup services_blob_db_app_glance_db_private App glance records
 * @ingroup services_blob_db
 * @brief Serialized app glance format stored in the app glance database.
 * @{
 */

/** @brief Version of the serialized glance format; records with another version are rejected. */
#define APP_GLANCE_DB_CURRENT_VERSION (1)

/**
 * @brief Maximum number of slices stored per glance; extra slices are trimmed on insert.
 *
 * Reduced in unit tests to keep test payloads small.
 */
#if UNITTEST
#define APP_GLANCE_DB_MAX_SLICES_PER_GLANCE (2)
#else
#define APP_GLANCE_DB_MAX_SLICES_PER_GLANCE (8)
#endif

/** @brief Number of glances the database is sized for. */
#define APP_GLANCE_DB_MAX_NUM_APP_GLANCES (50)

/** @brief Header of a serialized glance, the value of an app glance record. */
typedef struct PBL_PACKED SerializedAppGlanceHeader {
  /** Format version, ::APP_GLANCE_DB_CURRENT_VERSION. */
  uint8_t version;
  /** Creation time; inserts must be strictly newer than the stored glance. */
  uint32_t creation_time;
  /** Serialized slices. */
  uint8_t data[];
} SerializedAppGlanceHeader;

/** @brief Header of a serialized glance slice. */
typedef struct PBL_PACKED SerializedAppGlanceSliceHeader {
  /** Size of the slice in bytes, header included. */
  uint16_t total_size;
  /** Slice type (@c AppGlanceSliceType). */
  uint8_t type;
  /** Number of serialized attributes in @ref data. */
  uint8_t num_attributes;
  /** Serialized attributes. */
  uint8_t data[];
} SerializedAppGlanceSliceHeader;

/**
 * @brief Minimum size of an icon and subtitle slice.
 *
 * Header plus the expiration time; icon and subtitle are optional.
 */
#define APP_GLANCE_DB_ICON_AND_SUBTITLE_SLICE_MIN_SIZE \
  (sizeof(SerializedAppGlanceSliceHeader) + sizeof(SerializedAttributeHeader) + sizeof(uint32_t))
/**
 * @brief Maximum size of an icon and subtitle slice.
 *
 * Header plus the expiration time, icon resource id and NUL-terminated subtitle attributes.
 */
#define APP_GLANCE_DB_ICON_AND_SUBTITLE_SLICE_MAX_SIZE                                \
  (sizeof(SerializedAppGlanceSliceHeader) + (sizeof(SerializedAttributeHeader) * 3) + \
   sizeof(uint32_t) + sizeof(uint32_t) + ATTRIBUTE_APP_GLANCE_SUBTITLE_MAX_LEN + 1)

/** @brief Minimum size of any slice. */
#define APP_GLANCE_DB_SLICE_MIN_SIZE (APP_GLANCE_DB_ICON_AND_SUBTITLE_SLICE_MIN_SIZE)
/** @brief Maximum size of any slice. */
#define APP_GLANCE_DB_SLICE_MAX_SIZE (APP_GLANCE_DB_ICON_AND_SUBTITLE_SLICE_MAX_SIZE)

/** @} */
