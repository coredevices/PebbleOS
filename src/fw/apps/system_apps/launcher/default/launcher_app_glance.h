/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "launcher_app_glance_service.h"

#include "applib/ui/kino/kino_reel.h"
#include "services/normal/app_glances/app_glance_service.h"
#include "util/uuid.h"

//! Forward declaration
typedef struct LauncherAppGlance LauncherAppGlance;

//! Called when a launcher app glance's current slice has been updated. The glance will
//! automatically be redrawn after this function is called.
//! @param The glance whose current slice has been updated
typedef void (*LauncherAppGlanceCurrentSliceUpdated)(LauncherAppGlance *glance);

typedef struct LauncherAppGlanceHandlers {
  LauncherAppGlanceCurrentSliceUpdated current_slice_updated;
} LauncherAppGlanceHandlers;

struct LauncherAppGlance {
  //! The UUID of the app the launcher app glance represents
  Uuid uuid;
  //! The reel that implements how the launcher app glance should be drawn
  KinoReel *reel;
  //! Size of the area in which the launcher app glance expects to draw itself
  GSize size;
  //! Whether or not the launcher app glance is currently highlighted
  bool is_highlighted;
  //! Whether or not the launcher app glance should consider slices
  bool should_consider_slices;
  //! The current slice that should be drawn in the launcher app glance
  AppGlanceSliceInternal current_slice;
  //! The launcher app glance service that created the glance; used by the glance to notify the
  //! service that the glance needs to be redrawn
  LauncherAppGlanceService *service;
  //! Callback handlers for the launcher app glance
  LauncherAppGlanceHandlers handlers;
};

//! Initialize a launcher app glance.
//! @param glance The glance to initialize
//! @param uuid The UUID of the app
//! @param impl The KinoReel implementation for the glance
//! @param should_consider_slices Whether or not the glance should consider slices
//! @param handlers Optional handlers to use with the glance
void launcher_app_glance_init(LauncherAppGlance *glance, const Uuid *uuid, KinoReel *impl,
                              bool should_consider_slices,
                              const LauncherAppGlanceHandlers *handlers);

//! Update the current slice of the launcher app glance as well as the icon if the slice needs to
//! change it.
//! @param glance The glance for which to update the current slice
void launcher_app_glance_update_current_slice(LauncherAppGlance *glance);

//! Draw the provided launcher app glance.
//! @param ctx The graphics context to use when drawing the glance
//! @param frame The frame in which to draw the glance
//! @param glance The glance to draw
//! @param is_highlighted Whether or not the glance should be drawn highlighted
void launcher_app_glance_draw(GContext *ctx, const GRect *frame, LauncherAppGlance *glance,
                              bool is_highlighted);

//! Notify the launcher app glance's service that its content has changed.
//! @param glance The glance that has changed
void launcher_app_glance_notify_service_glance_changed(LauncherAppGlance *glance);

//! Destroy the provided launcher app glance.
//! @param glance The glance to destroy
void launcher_app_glance_destroy(LauncherAppGlance *glance);
