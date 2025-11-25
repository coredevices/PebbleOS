/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "shell_sdk.h"

#include "apps/core_apps/panic_window_app.h"
#include "apps/sdk/sdk_app.h"
#include "apps/system_app_ids.h"
#include "apps/system_apps/launcher/launcher_app.h"
#include "kernel/panic.h"
#include "process_management/app_install_manager.h"
#include "process_management/app_manager.h"
#include "shell/system_app_state_machine.h"
#include "shell/sdk/watchface.h"

//! Whether to return to the watchface instead of the launcher upon exiting an app.
static bool s_rooted_in_watchface = false;

const PebbleProcessMd *system_app_state_machine_system_start(void) {
  if (launcher_panic_get_current_error() != 0) {
    return panic_app_get_app_info();
  }

#if CAPABILITY_HAS_SDK_SHELL4
  const AppInstallId watchface_app_id = watchface_get_default_install_id();
  if (watchface_app_id != INSTALL_ID_INVALID) {
    return app_install_get_md(watchface_app_id, false /* worker */);
  }
#endif

  return sdk_app_get_info();
}

#if CAPABILITY_HAS_SDK_SHELL4
//! @return True if the currently running app is an installed watchface
static bool prv_current_app_is_watchface(void) {
  return app_install_is_watchface(app_manager_get_current_app_id());
}

AppInstallId system_app_state_machine_get_last_registered_app(void) {
  // If we're rooted in the watchface but we're not the watchface itself, or the launcher
  // is closing, we should launch the watchface.
  if ((s_rooted_in_watchface && !prv_current_app_is_watchface()) ||
      (app_manager_get_current_app_md() == launcher_menu_app_get_app_info())) {
    return watchface_get_default_install_id();
  }

  return APP_ID_LAUNCHER_MENU;
}

const PebbleProcessMd* system_app_state_machine_get_default_app(void) {
  return launcher_menu_app_get_app_info();
}

#else
AppInstallId system_app_state_machine_get_last_registered_app(void) {
  return APP_ID_SDK;
}

const PebbleProcessMd* system_app_state_machine_get_default_app(void) {
  return sdk_app_get_info();
}
#endif

void system_app_state_machine_register_app_launch(AppInstallId app_id) {
  if (app_install_id_from_app_db(app_id)) {
    shell_sdk_set_last_installed_app(app_id);
  }

#if CAPABILITY_HAS_SDK_SHELL4
  if (app_id == APP_ID_LAUNCHER_MENU) {
    s_rooted_in_watchface = false;
  } else if (app_install_is_watchface(app_id)) {
    s_rooted_in_watchface = true;
  }
  // Other app launches don't modify our root so just ignore them.
#endif
}

void system_app_state_machine_panic(void) {
  if (app_manager_is_initialized()) {
    app_manager_launch_new_app(&(AppLaunchConfig) {
      .md = panic_app_get_app_info(),
    });
  }
}
