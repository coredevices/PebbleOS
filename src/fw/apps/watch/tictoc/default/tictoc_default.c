/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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
