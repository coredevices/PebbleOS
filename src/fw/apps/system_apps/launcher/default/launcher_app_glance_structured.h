/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "launcher_app_glance.h"

#include "applib/ui/kino/kino_reel.h"
#include "apps/system_apps/timeline/text_node.h"
#include "util/uuid.h"

#define LAUNCHER_APP_GLANCE_STRUCTURED_ICON_MAX_SIZE \
    (GSize(ATTRIBUTE_ICON_TINY_SIZE_PX, ATTRIBUTE_ICON_TINY_SIZE_PX))
#define LAUNCHER_APP_GLANCE_STRUCTURED_ICON_LEGACY_MAX_SIZE \
    (GSize(28, 28))

//! Forward declaration
typedef struct LauncherAppGlanceStructured LauncherAppGlanceStructured;

//! Function used to get the title to display in the structured launcher app glance.
//! @param structured_glance The structured glance for which to get the title
//! @return The title to display in the structured glance; will be copied so can be short-lived
typedef const char *(*LauncherAppGlanceStructuredTitleGetter)
    (LauncherAppGlanceStructured *structured_glance);

//! Function used to create subtitle text nodes for the structured launcher app glance.
//! @param structured_glance The structured glance for which to create a text node
//! @return The text node the structured glance should use
typedef GTextNode *(*LauncherAppGlanceStructuredTextNodeConstructor)
    (LauncherAppGlanceStructured *structured_glance);

//! Function called when the structured launcher app glance is being destroyed.
//! @param structured_glance The structured glance that is being destroyed
//! @note This function should NOT free the structured glance; only deinit impl-specific things
typedef void (*LauncherAppGlanceStructuredDestructor)
    (LauncherAppGlanceStructured *structured_glance);

//! Function called to request the icon that should be drawn in the structured glance.
//! @param structured_glance The structured glance requesting the icon to draw
//! @return The icon to draw in the structured glance
typedef KinoReel *(*LauncherAppGlanceStructuredIconGetter)
    (LauncherAppGlanceStructured *structured_glance);

typedef struct LauncherAppGlanceStructuredImpl {
  //! Base handlers for the underlying LauncherAppGlance of the structured glance
  LauncherAppGlanceHandlers base_handlers;
  //! Called to get the icon to draw in the structured glance
  LauncherAppGlanceStructuredIconGetter get_icon;
  //! Called to create the title text node for the structured glance; must return a valid text node
  LauncherAppGlanceStructuredTitleGetter get_title;
  //! Called to create the subtitle text node for the structured glance
  LauncherAppGlanceStructuredTextNodeConstructor create_subtitle_node;
  //! Called when the structured glance is being destroyed; should NOT free the structured glance
  LauncherAppGlanceStructuredDestructor destructor;
} LauncherAppGlanceStructuredImpl;

struct LauncherAppGlanceStructured {
  //! The underlying launcher app glance
  LauncherAppGlance glance;
  //! The implementation of the structured app glance
  const LauncherAppGlanceStructuredImpl *impl;
  //! The user-provided data for the structured app glance's implementation
  void *data;
  //! Cached title font that will be used when drawing the structured app glance
  GFont title_font;
  //! Cached subtitle font that will be used when drawing the structured app glance
  GFont subtitle_font;
  // Cached text layout used when calculating the width of the subtitle during scrolling
  TextLayoutExtended subtitle_scroll_calc_text_layout;
  //! Optional implementation-provided dynamic text node update callback for the subtitle
  GTextNodeTextDynamicUpdate subtitle_update;
  //! Whether or not selection animations should be disabled for this structured app glance
  bool selection_animation_disabled;
  //! Current cumulative elapsed time (in milliseconds) of the glance's selection animation
  uint32_t selection_animation_elapsed_ms;
  //! Duration (in milliseconds) of the glance's selection animation
  uint32_t selection_animation_duration_ms;
  //! Maximum size an icon may have
  GSize icon_max_size;
  //! Horizontal margin for the icon
  int32_t icon_horizontal_margin;
};

_Static_assert((offsetof(LauncherAppGlanceStructured, glance) == 0),
               "LauncherAppGlance is not the first field of LauncherAppGlanceStructured");

//! Create a structured launcher app glance for the provided app menu node.
//! @param uuid The UUID of the app for which to initialize this structured glance
//! @param impl The implementation of the structured glance
//! @param should_consider_slices Whether or not the structured glance should consider slices
//! @param data Custom data to use in the implementation of the structured glance
LauncherAppGlanceStructured *launcher_app_glance_structured_create(
    const Uuid *uuid, const LauncherAppGlanceStructuredImpl *impl, bool should_consider_slices,
    void *data);

//! Get the user-provided data for the implementation of a structured launcher app glance.
//! @param structured_glance The structured glance for which to get the user-provided data
//! @return The user-provided data
void *launcher_app_glance_structured_get_data(LauncherAppGlanceStructured *structured_glance);

//! Get the highlight color that should be used for the provided structured launcher app glance.
//! @param structured_glance The structured glance for which to get the highlight color
//! @return The highlight color to use when drawing the structured glance
GColor launcher_app_glance_structured_get_highlight_color(
    LauncherAppGlanceStructured *structured_glance);

//! Draw an icon in the structured launcher app glance.
//! @param structured_glance The structured glance in which to draw an icon
//! @param ctx The graphics context to use when drawing the icon
//! @param icon The icon to draw
//! @param origin The origin at which to draw the icon
void launcher_app_glance_structured_draw_icon(LauncherAppGlanceStructured *structured_glance,
                                              GContext *ctx, KinoReel *icon, GPoint origin);

//! Create a subtitle text node for a structured launcher app glance. It is expected that subclasses
//! of \ref LauncherAppGlanceStructured will use this function in their own custom subtitle node
//! creation functions they specify in their \ref LauncherAppGlanceStructuredImpl. Calling this
//! function saves the provided callback to the \ref LauncherAppGlanceStructured struct, thus you
//! should only call this once per structured glance implementation.
//! @param structured_glance The structured glance for which to create a subtitle text node
//! @param update Callback for updating the text buffer of the text node
//! @return The resulting subtitle text node, or NULL upon failure
GTextNode *launcher_app_glance_structured_create_subtitle_text_node(
    LauncherAppGlanceStructured *structured_glance, GTextNodeTextDynamicUpdate update);

//! Notify the structured launcher app glance's service that its content has changed.
//! @param structured_glance The structured glance that has changed
void launcher_app_glance_structured_notify_service_glance_changed(
    LauncherAppGlanceStructured *structured_glance);

//! Change the icon max size, and adjust related settings.
//! @param structured_glance The structured glance for which to change the icon size
//! @param new_size The new maximum size allowed for the icon
void launcher_app_glance_structured_set_icon_max_size(
    LauncherAppGlanceStructured *structured_glance, GSize new_size);
