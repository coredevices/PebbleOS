/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#if defined(CONFIG_UI_DEBUG) && defined(CONFIG_SHELL)

#include "ui.h"
#include <applib/ui/app_window_stack.h>
#include <kernel/ui/modals/modal_manager.h>

#include <pbl/shell/shell.h>

extern void text_layer_update_proc(TextLayer *text_layer, GContext *ctx);
extern void action_bar_update_proc(ActionBarLayer *action_bar, GContext *ctx);
extern void bitmap_layer_update_proc(BitmapLayer *image, GContext *ctx);
extern void inverter_layer_update_proc(InverterLayer *inverter, GContext *ctx);
extern void menu_layer_update_proc(Layer *scroll_content_layer, GContext *ctx);
extern void path_layer_update_proc(PathLayer *path_layer, GContext *ctx);
extern void progress_layer_update_proc(ProgressLayer *progress_layer, GContext *ctx);
extern void rot_bitmap_layer_update_proc(RotBitmapLayer *image, GContext *ctx);
extern void scroll_layer_draw_shadow_sublayer(Layer *shadow_sublayer, GContext *ctx);
extern void window_do_layer_update_proc(Layer *layer, GContext *ctx);

static const char *prv_guess_type(Layer *layer) {
  if (layer == NULL) {
    return "NULL";
  };

  if (layer->update_proc == (LayerUpdateProc)text_layer_update_proc) {
    return "TextLayer";
  } else if (layer->update_proc == (LayerUpdateProc)action_bar_update_proc) {
    return "ActionBarLayer";
  } else if (layer->update_proc == (LayerUpdateProc)bitmap_layer_update_proc) {
    return "BitmapLayer";
  } else if (layer->update_proc == (LayerUpdateProc)inverter_layer_update_proc) {
    return "InverterLayer";
  } else if (layer->update_proc == (LayerUpdateProc)menu_layer_update_proc) {
    return "MenuLayer";
  } else if (layer->update_proc == (LayerUpdateProc)path_layer_update_proc) {
    return "PathLayer";
  } else if (layer->update_proc == (LayerUpdateProc)progress_layer_update_proc) {
    return "ProgressLayer";
  } else if (layer->update_proc == (LayerUpdateProc)rot_bitmap_layer_update_proc) {
    return "RotBitmapLayer";
  } else if (layer->update_proc == (LayerUpdateProc)scroll_layer_draw_shadow_sublayer) {
    return "(ScrollLayer's shadow) Layer";
  } else if (((ScrollLayer *)layer)->shadow_sublayer.update_proc ==
             scroll_layer_draw_shadow_sublayer) {
    return "ScrollLayer";
  } else if (layer->update_proc == (LayerUpdateProc)window_do_layer_update_proc) {
    return "Window";
  } else if (layer->update_proc == NULL) {
    return "Layer";
  } else {
    return "Custom Layer";
  }
}

static void prv_dump_level(const struct pbl_shell *sh, Layer *node, uint8_t indentation_level);

static void prv_dump_node(const struct pbl_shell *sh, Layer *node, uint8_t indentation_level) {
  pbl_shell_print(sh, "%*s(%s*) %p b:{{%i, %i}, {%i, %i}} f:{{%i, %i}, {%i, %i}} c:%u h:%u w:%p",
                  (indentation_level * 2), "", prv_guess_type(node), node, node->bounds.origin.x,
                  node->bounds.origin.y, node->bounds.size.w, node->bounds.size.h,
                  node->frame.origin.x, node->frame.origin.y, node->frame.size.w,
                  node->frame.size.h, node->clips, node->hidden, node->window);
  if (node->first_child) {
    prv_dump_level(sh, node->first_child, indentation_level + 1);
  }
}

static void prv_dump_level(const struct pbl_shell *sh, Layer *node, uint8_t indentation_level) {
  while (node) {
    prv_dump_node(sh, node, indentation_level);
    node = node->next_sibling;
  }
}

static int prv_cmd_dump(const struct pbl_shell *sh, size_t argc, char **argv) {
  Window *window = modal_manager_get_top_window();
  if (!window) {
    window = app_window_stack_get_top_window();
    if (window == NULL) {
      return 0;
    }
  }
  const char *window_name = window_get_debug_name(window);
  if (window_name) {
    pbl_shell_print(sh, "%s", window_name);
  }
  prv_dump_level(sh, window_get_root_layer(window), 0);
  return 0;
}

PBL_SHELL_SUBCMD_ADD(sub_ui, dump, NULL, "Dump the layer tree of the top window", prv_cmd_dump, 0,
                     0);
#endif
