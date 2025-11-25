/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "dialog.h"

#include "applib/ui/window.h"
#include "applib/applib_malloc.auto.h"

#include <string.h>

void dialog_set_fullscreen(Dialog *dialog, bool is_fullscreen) {
  window_set_fullscreen(&dialog->window, is_fullscreen);
}

void dialog_show_status_bar_layer(Dialog *dialog, bool show_status_layer) {
  dialog->show_status_layer = show_status_layer;
}

void dialog_set_text(Dialog *dialog, const char *text) {
  dialog_set_text_buffer(dialog, NULL, false);
  uint16_t len = strlen(text);
  dialog->is_buffer_owned = true;
  dialog->buffer = applib_malloc(len + 1);
  strncpy(dialog->buffer, text, len + 1);
  text_layer_set_text(&dialog->text_layer, dialog->buffer);
}

void dialog_set_text_buffer(Dialog *dialog, char *buffer, bool take_ownership) {
  if (dialog->buffer && dialog->is_buffer_owned) {
    applib_free(dialog->buffer);
  }
  dialog->is_buffer_owned = take_ownership;
  dialog->buffer = buffer;
}

void dialog_set_text_color(Dialog *dialog, GColor text_color) {
  dialog->text_color = PBL_IF_COLOR_ELSE(text_color, GColorBlack);
  text_layer_set_text_color(&dialog->text_layer, dialog->text_color);
}

void dialog_set_background_color(Dialog *dialog, GColor background_color) {
  window_set_background_color(&dialog->window, PBL_IF_COLOR_ELSE(background_color, GColorWhite));
}

void dialog_set_icon(Dialog *dialog, uint32_t icon_id) {
  if (dialog->icon_id == icon_id) {
    // Why bother destroying and then recreating the same icon?
    // Restart the animation to preserve behavior.
    kino_layer_rewind(&dialog->icon_layer);
    kino_layer_play(&dialog->icon_layer);
    return;
  }

  dialog->icon_id = icon_id;
  if (window_is_loaded(&dialog->window)) {
    kino_layer_set_reel_with_resource(&dialog->icon_layer, icon_id);
  }
}

void dialog_set_icon_animate_direction(Dialog *dialog, DialogIconAnimationDirection direction) {
  dialog->icon_anim_direction = direction;
}

void dialog_set_vibe(Dialog *dialog, bool vibe_on_show) {
  dialog->vibe_on_show = vibe_on_show;
}

void dialog_set_timeout(Dialog *dialog, uint32_t timeout) {
  dialog->timeout = timeout;
}

void dialog_set_callbacks(Dialog *dialog, const DialogCallbacks *callbacks,
                          void *callback_context) {
  if (!callbacks) {
    dialog->callbacks = (DialogCallbacks) {};
    return;
  }

  dialog->callbacks = *callbacks;
  dialog->callback_context = callback_context;
}

void dialog_set_destroy_on_pop(Dialog *dialog, bool destroy_on_pop) {
  dialog->destroy_on_pop = destroy_on_pop;
}

void dialog_appear(Dialog *dialog) {
  KinoLayer *icon_layer = &dialog->icon_layer;
  if (kino_layer_get_reel(icon_layer)) {
    kino_layer_play(icon_layer);
  }
}
