/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/app_timer.h"
#include "applib/event_service_client.h"
#include "applib/ui/kino/kino_player.h"
#include "process_management/app_menu_data_source.h"
#include "util/list.h"

//! Handler called when a glance in the service's cache changes, either because a glance's slice
//! expired or a glance was reloaded.
//! @param context Context provided when calling launcher_app_glance_service_set_handlers()
typedef void (*LauncherAppGlanceServiceGlanceChangedHandler)(void *context);

typedef struct LauncherAppGlanceServiceHandlers {
  LauncherAppGlanceServiceGlanceChangedHandler glance_changed;
} LauncherAppGlanceServiceHandlers;

typedef struct LauncherAppGlanceService {
  //! Cache of launcher app glances
  ListNode *glance_cache;
  //! Event service info used to subscribe to glance reload events
  EventServiceInfo glance_event_info;
  //! Client handlers set via launcher_app_glance_service_set_handlers()
  LauncherAppGlanceServiceHandlers handlers;
  //! Context for the handlers set via launcher_app_glance_service_set_handlers()
  void *handlers_context;
  //! The Unix epoch UTC timestamp of the next expiring slice of any of the glances in the cache
  time_t next_slice_expiration_time;
  //! App timer used for updating glances when a slice of a glance in the cache expires
  AppTimer *slice_expiration_timer;
  //! A generic icon to use for generic glances that can't otherwise load an icon
  KinoReel *generic_glance_icon;
  //! The resource ID of the generic glance icon
  uint32_t generic_glance_icon_resource_id;
  //! A \ref KinoReelPlayer for the currently selected glance
  KinoPlayer glance_reel_player;
} LauncherAppGlanceService;

//! Initialize the provided launcher app glance service.
//! @param service The launcher app glance service to initialize
//! @param generic_glance_icon_resource_id A resource ID to use if a generic launcher app glance
//! does not otherwise have an icon to draw
void launcher_app_glance_service_init(LauncherAppGlanceService *service,
                                      uint32_t generic_glance_icon_resource_id);

void launcher_app_glance_service_set_handlers(LauncherAppGlanceService *service,
                                              const LauncherAppGlanceServiceHandlers *handlers,
                                              void *context);

//! Deinitialize the provided launcher app glance service.
//! @param service The launcher app glance service to deinitialize
void launcher_app_glance_service_deinit(LauncherAppGlanceService *service);

//! Draw the launcher app glance for the provided app node.
//! @param service The service to use to draw the launcher app glance
//! @param ctx The graphics context to use to draw the launcher app glance
//! @param frame The frame in which to draw the launcher app glance
//! @param is_highlighted Whether or not the launcher app glance should be drawn highlighted
//! @param node The \ref AppMenuNode of the app whose glance we should draw
void launcher_app_glance_service_draw_glance_for_app_node(LauncherAppGlanceService *service,
                                                          GContext *ctx, const GRect *frame,
                                                          bool is_highlighted, AppMenuNode *node);

//! Rewind any glance being played by the provided launcher app glance service.
//! @param service The service for which to rewind any playing glance
void launcher_app_glance_service_rewind_current_glance(LauncherAppGlanceService *service);

//! Pause any glance being played by the provided launcher app glance service.
//! @param service The service for which to pause any playing glance
void launcher_app_glance_service_pause_current_glance(LauncherAppGlanceService *service);

//! Start playing the current glance for the provided launcher app glance service.
//! @param service The service for which to play the current glance
void launcher_app_glance_service_play_current_glance(LauncherAppGlanceService *service);

//! Play the launcher app glance for the provided app node.
//! @param service The service to use to play the launcher app glance
//! @param node The \ref AppMenuNode for the glance to play
void launcher_app_glance_service_play_glance_for_app_node(LauncherAppGlanceService *service,
                                                          AppMenuNode *node);

//! Notify the service that a launcher app glance in its cache changed.
//! @param service The service to notify
void launcher_app_glance_service_notify_glance_changed(LauncherAppGlanceService *service);
