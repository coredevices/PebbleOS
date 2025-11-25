/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "services/normal/blob_db/app_glance_db_private.h"
#include "services/normal/timeline/attribute.h"
#include "util/attributes.h"
#include "util/time/time.h"
#include "util/uuid.h"

typedef enum AppGlanceSliceType {
  AppGlanceSliceType_IconAndSubtitle = 0,

  AppGlanceSliceTypeCount
} AppGlanceSliceType;

//! We name this "internal" so it won't conflict with the AppGlanceSlice struct we export in the SDK
#if UNITTEST
// Memory comparisons in unit tests won't work unless we pack the struct
typedef struct PACKED AppGlanceSliceInternal {
#else
typedef struct AppGlanceSliceInternal {
#endif
  AppGlanceSliceType type;
  time_t expiration_time;
  union {
    //! Add more structs to this union as we introduce new app glance slice types
    struct {
      uint32_t icon_resource_id;
      char template_string[ATTRIBUTE_APP_GLANCE_SUBTITLE_MAX_LEN + 1];
    } icon_and_subtitle;
  };
} AppGlanceSliceInternal;

typedef struct AppGlance {
  size_t num_slices;
  AppGlanceSliceInternal slices[APP_GLANCE_DB_MAX_SLICES_PER_GLANCE];
} AppGlance;

//! Initializes an AppGlance.
void app_glance_service_init_glance(AppGlance *glance);

//! Initializes the app glance service.
void app_glance_service_init(void);

//! Returns true if the current slice was successfully copied to slice_out.
//! Returns false if all slices in the glance have expired or if an error occurred.
bool app_glance_service_get_current_slice(const Uuid *app_uuid, AppGlanceSliceInternal *slice_out);
