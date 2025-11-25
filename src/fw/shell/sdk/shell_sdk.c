/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "shell_sdk.h"

#include "process_management/app_install_manager.h"

AppInstallId s_last_installed_app = INSTALL_ID_INVALID;

AppInstallId shell_sdk_get_last_installed_app(void) {
  return s_last_installed_app;
}

void shell_sdk_set_last_installed_app(AppInstallId app_id) {
  s_last_installed_app = app_id;
}

bool shell_sdk_last_installed_app_is_watchface() {
  return app_install_is_watchface(shell_sdk_get_last_installed_app());
}
