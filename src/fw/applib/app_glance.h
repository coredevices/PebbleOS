/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "services/normal/app_glances/app_glance_service.h"
#include "util/time/time.h"

#include <stdint.h>

//! @addtogroup Foundation
//! @{
//!   @addtogroup AppGlance App Glance
//!   \brief API for the application to modify its glance.
//!
//!   @{

//! The ID of a published app resource defined within the publishedMedia section of package.json.
typedef uint32_t PublishedId;

//! Can be used for the expiration_time of an \ref AppGlanceSlice so that the slice never expires.
#define APP_GLANCE_SLICE_NO_EXPIRATION ((time_t)0)

//! Can be used for the icon of an \ref AppGlanceSlice so that the slice displays the app's default
//! icon.
#define APP_GLANCE_SLICE_DEFAULT_ICON ((PublishedId)0)

//! An app's glance can change over time as defined by zero or more app glance slices that each
//! describe the state of the app glance at a particular point in time. Slices are displayed in the
//! order they are added, and they are removed at the specified expiration time.
typedef struct AppGlanceSlice {
  //! Describes how the slice should be visualized in the app's glance in the launcher.
  struct {
    //! The published resource ID of the bitmap icon to display in the app's glance. Use \ref
    //! APP_GLANCE_SLICE_DEFAULT_ICON to use the app's default bitmap icon.
    PublishedId icon;
    //! A template string to visualize in the app's glance. The string will be copied, so it is safe
    //! to destroy after adding the slice to the glance. Use NULL if no string should be displayed.
    const char *subtitle_template_string;
  } layout;
  //! The UTC time after which this slice should no longer be shown in the app's glance. Use \ref
  //! APP_GLANCE_SLICE_NO_EXPIRATION if the slice should never expire.
  time_t expiration_time;
} AppGlanceSlice;

//! Bitfield enum describing the result of trying to add an AppGlanceSlice to an app's glance.
typedef enum AppGlanceResult {
  //! The slice was successfully added to the app's glance.
  APP_GLANCE_RESULT_SUCCESS = 0,
  //! The subtitle_template_string provided in the slice was invalid.
  APP_GLANCE_RESULT_INVALID_TEMPLATE_STRING = 1 << 0,
  //! The subtitle_template_string provided in the slice was longer than 150 bytes.
  APP_GLANCE_RESULT_TEMPLATE_STRING_TOO_LONG = 1 << 1,
  //! The icon provided in the slice was invalid.
  APP_GLANCE_RESULT_INVALID_ICON = 1 << 2,
  //! The provided slice would exceed the app glance's slice capacity.
  APP_GLANCE_RESULT_SLICE_CAPACITY_EXCEEDED = 1 << 3,
  //! The expiration_time provided in the slice expires in the past.
  APP_GLANCE_RESULT_EXPIRES_IN_THE_PAST = 1 << 4,
  //! The \ref AppGlanceReloadSession provided was invalid.
  APP_GLANCE_RESULT_INVALID_SESSION = 1 << 5,
} AppGlanceResult;

//! A session variable that is provided in an \ref AppGlanceReloadCallback and must be used when
//! adding slices to the app's glance via \ref app_glance_add_slice.
typedef struct AppGlanceReloadSession AppGlanceReloadSession;
struct AppGlanceReloadSession {
  AppGlance *glance;
};

//! Add a slice to the app's glance. This function will only succeed if called with a valid
//! \ref AppGlanceReloadSession that is provided in an \ref AppGlanceReloadCallback.
//! @param session The session variable provided in an \ref AppGlanceReloadCallback
//! @param slice The slice to add to the app's glance
//! @return The result of trying to add the slice to the app's glance
AppGlanceResult app_glance_add_slice(AppGlanceReloadSession *session, AppGlanceSlice slice);

//! User-provided callback for reloading the slices in the app's glance.
//! @param session A session variable that must be passed to \ref app_glance_add_slice when adding
//! slices to the app's glance; it becomes invalid when the \ref AppGlanceReloadCallback returns
//! @param limit The number of entries that can be added to the app's glance
//! @param context User-provided context provided when calling \ref app_glance_reload()
typedef void (*AppGlanceReloadCallback)(AppGlanceReloadSession *session, size_t limit,
                                        void *context);

//! Clear any existing slices in the app's glance and trigger a reload via the provided callback.
//! @param callback A function that will be called to add new slices to the app's glance; even if
//! the provided callback is NULL, any existing slices will still be cleared from the app's glance
//! @param context User-provided context that will be passed to the callback
void app_glance_reload(AppGlanceReloadCallback callback, void *context);

//!   @} // group AppGlance
//! @} // group Foundation
