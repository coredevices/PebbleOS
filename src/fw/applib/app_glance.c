/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "app_glance.h"

#include "applib/template_string.h"
#include "apps/system_apps/launcher/default/launcher_app_glance_generic.h"
#include "process_state/app_state/app_state.h"
#include "resource/resource_ids.auto.h"
#include "services/normal/blob_db/app_glance_db_private.h"
#include "services/normal/timeline/attribute.h"
#include "services/normal/timeline/timeline_resources.h"
#include "syscall/syscall.h"
#include "util/string.h"
#include "util/uuid.h"

AppGlanceResult app_glance_add_slice(AppGlanceReloadSession *session, AppGlanceSlice slice) {
  #if !CAPABILITY_HAS_APP_GLANCES
    return APP_GLANCE_RESULT_INVALID_SESSION;
  #endif
  AppGlanceResult result = APP_GLANCE_RESULT_SUCCESS;

  AppGlance *glance = app_state_get_glance();

  // We use the glance set in the session as a crude way of validating the session
  // Bail out if the session is invalid
  if (!session || (glance != session->glance)) {
    result |= APP_GLANCE_RESULT_INVALID_SESSION;
    return result;
  }

  // From here on to the end of the function we can accumulate multiple failures in `result`

  // Check if this slice would put us over the max slices per glance
  if (glance->num_slices >= APP_GLANCE_DB_MAX_SLICES_PER_GLANCE) {
    result |= APP_GLANCE_RESULT_SLICE_CAPACITY_EXCEEDED;
  }

  // Check if this slice's icon is a valid resource (but only if it's not the value that specifies
  // that the app's default icon should be used)
  if (slice.layout.icon != APP_GLANCE_SLICE_DEFAULT_ICON) {
    Uuid app_uuid;
    sys_get_app_uuid(&app_uuid);
    const TimelineResourceInfo timeline_resource_info = (TimelineResourceInfo) {
      .app_id = &app_uuid,
      .res_id = (TimelineResourceId)slice.layout.icon,
    };
    AppResourceInfo app_resource_info = (AppResourceInfo) {};
    sys_timeline_resources_get_id(&timeline_resource_info,
                                  LAUNCHER_APP_GLANCE_GENERIC_ICON_SIZE_TYPE, &app_resource_info);
    if (app_resource_info.res_id == RESOURCE_ID_INVALID) {
      result |= APP_GLANCE_RESULT_INVALID_ICON;
    }
  }

  // Check if the template string is too long (if it's not NULL)
  // Plus one for the null terminator, then plus one more to become too long
  const size_t template_string_too_long_size = ATTRIBUTE_APP_GLANCE_SUBTITLE_MAX_LEN + 1 + 1;
  if (slice.layout.subtitle_template_string &&
      strnlen(slice.layout.subtitle_template_string,
              template_string_too_long_size) == template_string_too_long_size) {
    result |= APP_GLANCE_RESULT_TEMPLATE_STRING_TOO_LONG;
  }

  // Check if the provided subtitle string is a valid template string, if it's present.
  const time_t current_time = sys_get_time();
  if (slice.layout.subtitle_template_string) {
    const TemplateStringVars template_string_vars = (TemplateStringVars) {
      .current_time = current_time,
    };
    TemplateStringError template_string_error = {0};
    if (!template_string_evaluate(slice.layout.subtitle_template_string, NULL, 0, NULL,
                                  &template_string_vars, &template_string_error)) {
      result |= APP_GLANCE_RESULT_INVALID_TEMPLATE_STRING;
    }
  }

  // Check if the slice would expire in the past (if it's not APP_GLANCE_SLICE_NO_EXPIRATION)
  if ((slice.expiration_time != APP_GLANCE_SLICE_NO_EXPIRATION) &&
      slice.expiration_time <= current_time) {
    result |= APP_GLANCE_RESULT_EXPIRES_IN_THE_PAST;
  }

  // If we haven't failed at this point, we're ready to add the slice to the glance!
  if (result == APP_GLANCE_RESULT_SUCCESS) {
    AppGlanceSliceInternal *slice_dest = &glance->slices[glance->num_slices];
    *slice_dest = (AppGlanceSliceInternal) {
      .expiration_time = slice.expiration_time,
      .type = AppGlanceSliceType_IconAndSubtitle,
      .icon_and_subtitle.icon_resource_id = slice.layout.icon,
    };
    // We already checked that the provided template string fits, so use strcpy instead of strncpy
    if (slice.layout.subtitle_template_string) {
      strcpy(slice_dest->icon_and_subtitle.template_string, slice.layout.subtitle_template_string);
    }
    glance->num_slices++;
  }

  return result;
}

void app_glance_reload(AppGlanceReloadCallback callback, void *context) {
  #if !CAPABILITY_HAS_APP_GLANCES
    return;
  #endif

  AppGlance *glance = app_state_get_glance();

  Uuid current_app_uuid;
  sys_get_app_uuid(&current_app_uuid);

  // Zero out the glance
  app_glance_service_init_glance(glance);

  if (callback) {
    // Create a "reload session" on the stack that wraps the glance, and then call the user callback
    AppGlanceReloadSession session = (AppGlanceReloadSession) {
      .glance = glance,
    };
    callback(&session, APP_GLANCE_DB_MAX_SLICES_PER_GLANCE, context);
  }

  sys_app_glance_update(&current_app_uuid, glance);
}
