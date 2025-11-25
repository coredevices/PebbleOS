/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pebble.h>

extern bool rocky_event_loop_with_resource(uint32_t resource_id);

int main(void) {
  Window *window = window_create();
  window_stack_push(window, false);
  rocky_event_loop_with_resource(RESOURCE_ID_JS_SNAPSHOT);
}
