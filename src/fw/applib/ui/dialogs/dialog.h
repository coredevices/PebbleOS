/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/app_timer.h"
#include "applib/ui/text_layer.h"
#include "applib/ui/status_bar_layer.h"
#include "applib/ui/kino/kino_layer.h"
#include "applib/ui/window.h"

#include <stdbool.h>

#define DIALOG_MAX_MESSAGE_LEN 140
#define DIALOG_IS_ANIMATED true

// TODO PBL-38106: Replace uses of DIALOG_TIMEOUT_DEFAULT with preferred_result_display_duration()
// The number of milliseconds it takes for the dialog to automatically go away if has_timeout is
// set to true.
#define DIALOG_TIMEOUT_DEFAULT (1000)
#define DIALOG_TIMEOUT_INFINITE (0)

struct Dialog;

typedef void (*DialogCallback)(void *context);

typedef enum {
  // most dialogs will be pushed. FromRight works best for that (it is default)
  DialogIconAnimateNone = 0,
  DialogIconAnimationFromRight,
  DialogIconAnimationFromLeft,
} DialogIconAnimationDirection;

typedef struct {
  DialogCallback load;
  DialogCallback unload;
} DialogCallbacks;

//! A newly created Dialog will have the following defaults:
//! * Fullscreen: True,
//! * Show Status Layer: False,
//! * Text Color: GColorBlack,
//! * Background Color: GColorLightGray (GColorWhite for tintin)
//! * Vibe: False

// Dialog object used as the core of other dialog types. The Dialog object shouldn't be used
// directly to create a dialog window. Instead, one of specific types that wraps a Dialog should
// be used, such as the SimpleDialog.
typedef struct Dialog {
  Window window;

  // Time out. The dialog can be configured to timeout after DIALOG_TIMEOUT_DURATION ms.
  uint32_t timeout;
  AppTimer *timer;

  // Buffer for the main text of the dialog.
  char *buffer;
  bool is_buffer_owned;

  // True if the dialog should vibrate when it opens, false otherwise.
  bool vibe_on_show;

  bool show_status_layer;
  StatusBarLayer status_layer;

  // Icon for the dialog.
  KinoLayer icon_layer;
  uint32_t icon_id;
  DialogIconAnimationDirection icon_anim_direction;

  // Text layer on which the main text goes.
  TextLayer text_layer;

  // Color of the dialog text.
  GColor text_color;

  // Callbacks and context for unloading the dialog. The user is allowed to set these callbacks to
  // perform actions (such as freeing resources) when the dialog window has appeared or is unloaded.
  // They are also useful if the user is wanted to change the KinoReel for the exit animation.
  DialogCallbacks callbacks;
  void *callback_context;

  bool destroy_on_pop;
} Dialog;

// If set to true, sets the dialog window to fullscreen.
void dialog_set_fullscreen(Dialog *dialog, bool is_fullscreen);

// If set to true, shows a status bar layer at the top of the dialog.
void dialog_show_status_bar_layer(Dialog *dialog, bool show_status_layer);

// Sets the dialog's main text.
// Allocates a buffer on the application heap to store the text. The dialog will retain ownership of
// the buffer and will free it if different text is set or a different buffer is specified with
// dialog_set_text_buffer.
void dialog_set_text(Dialog *dialog, const char *text);

// Sets the dialog's main text using the string in the buffer passed. Any buffer owned by the dialog
// will be freed when the dialog is unloaded or when another buffer or text (dialog_set_text) is
// supplied
void dialog_set_text_buffer(Dialog *dialog, char *buffer, bool take_ownership);

// Sets the color of the dialog's text.
// if SCREEN_COLOR_DEPTH_BITS == 1 then the color will always be set to black
void dialog_set_text_color(Dialog *dialog, GColor text_color);

// Sets the background color of the dialog window.
// if SCREEN_COLOR_DEPTH_BITS == 1 then the color will always be set to white
void dialog_set_background_color(Dialog *dialog, GColor background_color);

// Sets the icon displayed by the dialog.
void dialog_set_icon(Dialog *dialog, uint32_t icon_id);

// Sets the direction from which in the icon animates in.
void dialog_set_icon_animate_direction(Dialog *dialog, DialogIconAnimationDirection direction);

// If set to true, the dialog will emit a short vibe pulse when first opened.
void dialog_set_vibe(Dialog *dialog, bool vibe_on_show);

// Set the timeout of the dialog. Using DIALOG_TIMEOUT_DEFAULT will set the timeout to 1s, using
// DIALOG_TIMEOUT_INFINITE (0) will disable the timeout
void dialog_set_timeout(Dialog *dialog, uint32_t timeout);

// Allows the user to provide a custom callback and optionally a custom context for unloading the
// dialog. This callback will be called from the dialog's own unload function and can be used
// to clean up resources used by the dialog such as icons. If the unload context is NULL, the
// parent dialog object will be passed instead.
void dialog_set_callbacks(Dialog *dialog, const DialogCallbacks *callbacks,
                          void *callback_context);

// Enable or disable automatically destroying the dialog when it's popped.
void dialog_set_destroy_on_pop(Dialog *dialog, bool destroy_on_pop);
