/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#ifdef UI_DEBUG
#include "ui_nudge.h"

#include "console_internal.h"
#include "dbgserial.h"

#include "applib/ui/layer.h"
#include "applib/ui/ui_debug.h"
#include "applib/ui/app_window_stack.h"
#include "applib/ui/window_stack_private.h"

#include "kernel/events.h"

#include "services/common/compositor/compositor.h"
#include "util/string.h"

static Layer *s_nudge_layer = NULL;

static void prv_flush_kernel_main_cb(void *unused) {
  // FIXME compositor_flush();
}

void layer_debug_nudging_handle_character(char c, bool *should_context_switch) {
  GRect frame = s_nudge_layer->frame;
  GRect bounds = s_nudge_layer->bounds;
  switch (c) {
    case 0x3: // CTRL - C
      s_nudge_layer = NULL;
      // Back to log mode:
      serial_console_set_state(SERIAL_CONSOLE_STATE_LOGGING);
      // Dump window:
      command_dump_window();
      return;

    case 'A':
    case 'a':
      --frame.origin.x;
      break;

    case 'D':
    case 'd':
      ++frame.origin.x;
      break;

    case 'W':
    case 'w':
      ++frame.origin.y;
      break;

    case 'S':
    case 's':
      --frame.origin.y;
      break;

    case '[':
      --frame.size.w;
      bounds.size = frame.size;
      break;

    case ']':
      ++frame.size.w;
      bounds.size = frame.size;
      break;

    case '{':
      --frame.size.h;
      bounds.size = frame.size;
      break;

    case '}':
      ++frame.size.h;
      bounds.size = frame.size;
      break;

    default:
      break;
  }
  layer_set_frame(s_nudge_layer, &frame);
  layer_set_bounds(s_nudge_layer, &bounds);

  PebbleEvent event = {
    .type = PEBBLE_CALLBACK_EVENT,
    .callback = {
      .callback = prv_flush_kernel_main_cb,
      .data = NULL,
    },
  };
  *should_context_switch = event_put_isr(&event);
}

void command_layer_nudge(const char *address_str) {
  intptr_t address = str_to_address(address_str);
  if (address == -1) {
    return;
  }
  // Simple sanity check:
  if (((Layer *)address)->window != app_window_stack_get_top_window()) {
    dbgserial_putstr("Specify valid Layer address!");
    return;
  }
  s_nudge_layer = (Layer *)address;

  dbgserial_putstr("Layer nudging mode, CTRL-C to stop");
  dbgserial_putstr("Keys:\nWASD: Move frame.origin\n[]: Change frame.size.w & bounds.size.w\n{}: Change frame.size.h & bounds.size.h");
  serial_console_set_state(SERIAL_CONSOLE_STATE_LAYER_NUDGING);
}
#endif /* UI_DEBUG */
