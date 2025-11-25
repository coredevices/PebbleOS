/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "rocky_api.h"

#include <stddef.h>

#include "rocky_api_app_message.h"
#include "rocky_api_console.h"
#include "rocky_api_datetime.h"
#include "rocky_api_global.h"
#include "rocky_api_graphics.h"
#include "rocky_api_memory.h"
#include "rocky_api_preferences.h"
#include "rocky_api_tickservice.h"
#include "rocky_api_timers.h"
#include "rocky_api_watchinfo.h"

void rocky_api_watchface_init(void) {
  static const RockyGlobalAPI *const apis[] = {
#if !APPLIB_EMSCRIPTEN
    &APP_MESSAGE_APIS,
    &CONSOLE_APIS,
#endif

    &DATETIME_APIS,
    &GRAPHIC_APIS,

#if !APPLIB_EMSCRIPTEN
    &MEMORY_APIS,
    &PREFERENCES_APIS,
#endif

    &TICKSERVICE_APIS,

#if !APPLIB_EMSCRIPTEN
    &TIMER_APIS,
    &WATCHINFO_APIS,
#endif

    NULL,
  };
  rocky_global_init(apis);
}

void rocky_api_deinit(void) {
  rocky_global_deinit();
}
