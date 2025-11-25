/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "test_args_tx.h"
#include "test_args_rx.h"

#include "applib/app.h"
#include "kernel/event_loop.h"
#include "process_management/app_manager.h"
#include "process_management/app_install_manager.h"
#include "process_management/process_manager.h"
#include "system/logging.h"

static TestArgsData s_data;

static void prv_launch_receiver_callback(void *data) {
  AppInstallId id = app_install_get_id_for_uuid(&(test_args_receiver_get_app_info()->uuid));
  app_manager_put_launch_app_event(&(AppLaunchEventConfig) {
    .id = id,
    .common.args = data,
  });
}

static void s_main(void) {
  s_data.data = 0x43;
  PBL_LOG(LOG_LEVEL_DEBUG, "Launching again with argument: 0x%x", s_data.data);
  launcher_task_add_callback(prv_launch_receiver_callback, &s_data);
}

const PebbleProcessMd* test_args_sender_get_app_info() {
  static const PebbleProcessMdSystem test_args_sender_demo_app_info = {
    .common.uuid = {0xD1, 0x7E, 0x41, 0xAF, 0x40, 0x5E, 0x40, 0x76,
      0x82, 0xB5, 0x97, 0x71, 0x70, 0x52, 0x66, 0xBA},
    .common.main_func = s_main,
    .name = "Args Sender Demo"
  };
  return (const PebbleProcessMd*) &test_args_sender_demo_app_info;
}
