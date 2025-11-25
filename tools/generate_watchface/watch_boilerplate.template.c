/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

/**
 * The basis for a generated watch face
 */

#include "pebble_gen_defs.h"

#include "pebble.h"

#include <stdint.h>
#include <stdbool.h>

#include "pebble_gen_header.h"
#include "resource_ids.gen.h"


PBL_APP_INFO(PBL_GEN_VISIBLE_NAME_STR, PBL_GEN_COMPANY_NAME_STR);

static void handle_init(AppContextRef ctx) {
  (void)ctx;

  static Window s_window;
  window_init(&s_window, "Window Name");
  window_stack_push(&s_window);

  static Layer s_watch_layer;

  layer_init(&s_watch_layer, s_window.layer.frame);
  layer_add_child(&s_window.layer, &s_watch_layer);

  PBL_GEN_INIT(&s_watch_layer);
}

static void handle_render(AppContextRef ctx, PebbleRenderEvent *e) {
  (void)ctx;
  window_render(e->window, e->ctx);

  layer_mark_dirty(&e->window->layer);
}

static void handle_deinit(AppContextRef ctx) {
  (void)ctx;
  // TODO: let go of resources
}

void pbl_main(void* params) {
  PebbleAppHandlers s_handlers = {
    .init_handler = &handle_init,
    .render_handler = &handle_render,
    .deinit_handler = &handle_deinit
  };
  app_event_loop(params, &s_handlers);
}
