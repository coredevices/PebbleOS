/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <time.h>

#include <pbl/kernel/compiler.h>
#include <pbl/services/blob_db/app_glance_db_private.h>
#include <pbl/services/timeline/attribute.h>
#include <pbl/util/uuid.h>

/**
 * @defgroup services_app_glances App glances
 * @ingroup services
 * @brief App glances: per-app slices shown in the launcher.
 *
 * Glances are stored in the app glance database. The service emits a
 * @c PEBBLE_APP_GLANCE_EVENT when an app's glance changes and deletes the glance when the app is
 * removed from the app cache.
 * @{
 */

/** @brief Glance slice layout. */
typedef enum AppGlanceSliceType {
  /** Icon and subtitle. */
  AppGlanceSliceType_IconAndSubtitle = 0,

  /** Number of slice types. */
  AppGlanceSliceTypeCount
} AppGlanceSliceType;

/**
 * @brief Glance slice as stored by the firmware.
 *
 * Named "internal" to avoid a conflict with the SDK AppGlanceSlice.
 */
#if UNITTEST
// Memory comparisons in unit tests won't work unless we pack the struct
typedef struct PBL_PACKED AppGlanceSliceInternal {
#else
typedef struct AppGlanceSliceInternal {
#endif
  /** Layout of the slice. */
  AppGlanceSliceType type;
  /** Time after which the slice is no longer shown, or 0 if it never expires. */
  time_t expiration_time;
  // Add more structs to this union as we introduce new app glance slice types
  union {
    /** Data of @ref AppGlanceSliceType_IconAndSubtitle slices. */
    struct {
      /** Published resource id of the icon. */
      uint32_t icon_resource_id;
      /** Subtitle template string, zero terminated. */
      char template_string[ATTRIBUTE_APP_GLANCE_SUBTITLE_MAX_LEN + 1];
    } icon_and_subtitle;
  };
} AppGlanceSliceInternal;

/** @brief App glance: a list of slices. */
typedef struct AppGlance {
  /** Number of valid entries in @ref slices. */
  size_t num_slices;
  /** Slices. */
  AppGlanceSliceInternal slices[APP_GLANCE_DB_MAX_SLICES_PER_GLANCE];
} AppGlance;

/**
 * @brief Initialize a glance with no slices.
 *
 * @param[out] glance Glance to initialize, may be NULL.
 */
void app_glance_service_init_glance(AppGlance *glance);

/** @brief Initialize the service; subscribes to database and app cache events on KernelMain. */
void app_glance_service_init(void);

/**
 * @brief Get the current slice of an app's glance.
 *
 * The current slice is the unexpired one expiring soonest, or a never-expiring one if no other
 * is left.
 *
 * @param app_uuid App UUID.
 * @param[out] slice_out Current slice.
 * @return false if the app has no glance, all slices expired, or on error.
 */
bool app_glance_service_get_current_slice(const Uuid *app_uuid, AppGlanceSliceInternal *slice_out);

/** @} */
