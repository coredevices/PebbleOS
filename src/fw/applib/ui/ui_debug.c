/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#ifdef UI_DEBUG

#include "ui.h"
#include "applib/ui/app_window_stack.h"
#include "kernel/ui/modals/modal_manager.h"

#include "console/dbgserial.h"

extern void text_layer_update_proc(TextLayer *text_layer, GContext* ctx);
extern void action_bar_update_proc(ActionBarLayer *action_bar, GContext* ctx);
extern void bitmap_layer_update_proc(BitmapLayer *image, GContext* ctx);
extern void inverter_layer_update_proc(InverterLayer *inverter, GContext* ctx);
extern void menu_layer_update_proc(Layer *scroll_content_layer, GContext* ctx);
extern void path_layer_update_proc(PathLayer *path_layer, GContext* ctx);
extern void progress_layer_update_proc(ProgressLayer* progress_layer, GContext* ctx);
extern void rot_bitmap_layer_update_proc(RotBitmapLayer *image, GContext* ctx);
extern void scroll_layer_draw_shadow_sublayer(Layer *shadow_sublayer, GContext* ctx);
extern void window_do_layer_update_proc(Layer *layer, GContext* ctx);

const char *layer_debug_guess_type(Layer *layer) {
  if (layer == NULL) {
    return "NULL";
  };

  if (layer->update_proc == (LayerUpdateProc) text_layer_update_proc) {
    return "TextLayer";
  } else if (layer->update_proc == (LayerUpdateProc) action_bar_update_proc) {
    return "ActionBarLayer";
  } else if (layer->update_proc == (LayerUpdateProc) bitmap_layer_update_proc) {
    return "BitmapLayer";
  } else if (layer->update_proc == (LayerUpdateProc) inverter_layer_update_proc) {
    return "InverterLayer";
  } else if (layer->update_proc == (LayerUpdateProc) menu_layer_update_proc) {
    return "MenuLayer";
  } else if (layer->update_proc == (LayerUpdateProc) path_layer_update_proc) {
    return "PathLayer";
  } else if (layer->update_proc == (LayerUpdateProc) progress_layer_update_proc) {
    return "ProgressLayer";
  } else if (layer->update_proc == (LayerUpdateProc) rot_bitmap_layer_update_proc) {
    return "RotBitmapLayer";
  } else if (layer->update_proc == (LayerUpdateProc) scroll_layer_draw_shadow_sublayer) {
    return "(ScrollLayer's shadow) Layer";
  } else if (((ScrollLayer*)layer)->shadow_sublayer.update_proc == scroll_layer_draw_shadow_sublayer) {
    return "ScrollLayer";
  } else if (layer->update_proc == (LayerUpdateProc) window_do_layer_update_proc) {
    return "Window";
  } else if (layer->update_proc == NULL) {
    return "Layer";
  } else {
    return "Custom Layer";
  }
}

static void layer_dump_tree_node(Layer* node, uint8_t indentation_level, char *buffer, uint8_t buffer_size);

void layer_dump_level(Layer* node, uint8_t indentation_level, char *buffer, uint8_t buffer_size) {
  while (node) {
    layer_dump_tree_node(node, indentation_level, buffer, buffer_size);
    node = node->next_sibling;
  }
}

static void layer_dump_tree_node(Layer* node, uint8_t indentation_level, char *buffer, uint8_t buffer_size) {
  const bool hidden = node->hidden;
  const bool clips = node->clips;
  const char *layer_type_string = layer_debug_guess_type(node);
  dbgserial_putstr_fmt(buffer, buffer_size, "%*s(%s*) %p b:{{%i, %i}, {%i, %i}} f:{{%i, %i}, {%i, %i}} c:%u h:%u w:%p", (indentation_level * 2), "", layer_type_string, node, node->bounds.origin.x, node->bounds.origin.y, node->bounds.size.w, node->bounds.size.h, node->frame.origin.x, node->frame.origin.y, node->frame.size.w, node->frame.size.h, clips, hidden, node->window);
  if (node->first_child) {
    layer_dump_level(node->first_child, ++indentation_level, buffer, buffer_size);
  }
}

void layer_dump_tree(Layer* node) {
  const uint8_t buffer_size = 128;
  char buffer[buffer_size];
  layer_dump_level(node, 0, buffer, buffer_size);
}

void command_dump_window(void) {
  Window *window = modal_manager_get_top_window();
  if (!window) {
    window = app_window_stack_get_top_window();
    if (window == NULL) {
      return;
    }
  }
  const char *window_name = window_get_debug_name(window);
  if (window_name) {
    dbgserial_putstr(window_name);
  }
  layer_dump_tree(window_get_root_layer(window));
}
#endif /* UI_DEBUG */
