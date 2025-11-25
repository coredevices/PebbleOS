/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "applib/app.h"
#include "applib/rockyjs/rocky.h"
#include "applib/ui/app_window_stack.h"
#include "resource/resource_ids.auto.h"

void tictoc_main(void) {
  // Push a window so we don't exit
  Window *window = window_create();
  app_window_stack_push(window, false/*animated*/);
#if CAPABILITY_HAS_ROCKY_JS
  rocky_event_loop_with_system_resource(RESOURCE_ID_JS_TICTOC);
#else
  app_event_loop();
#endif
}
