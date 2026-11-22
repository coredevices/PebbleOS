/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <string.h>

#include <pbl/i18n/i18n.h>
#include <pbl/services/timeline/timeline.h>

#include <process_management/pebble_process_md.h>
#include <resource/resource_ids.auto.h>

#include "chat_ds.h"

void chat_ui_main(void);

// chat_ui.c is an SDK app's code: it expects its statics to start from their initial values on
// every launch, as an app's do when loaded. The linker script gathers its .data and .bss so they
// can be reset here.
extern uint8_t __chat_ui_data_start[], __chat_ui_data_end[], __chat_ui_data_load[];
extern uint8_t __chat_ui_bss_start[], __chat_ui_bss_end[];

static void prv_reset_ui_statics(void) {
  memcpy(__chat_ui_data_start, __chat_ui_data_load, __chat_ui_data_end - __chat_ui_data_start);
  memset(__chat_ui_bss_start, 0, __chat_ui_bss_end - __chat_ui_bss_start);
}

static void prv_main(void) {
  prv_reset_ui_statics();
  chat_ds_init();
  chat_ui_main();
  chat_ds_deinit();
}

const PebbleProcessMd *chat_app_get_info(void) {
  static const PebbleProcessMdSystem s_chat_app_info = {
    .common =
        {
          .main_func = prv_main,
          .uuid = UUID_CHAT,
        },
    .name = pbl_i18n_noop("Chat"),
    .icon_resource_id = RESOURCE_ID_CHAT_MENU_ICON,
  };
  return (const PebbleProcessMd *)&s_chat_app_info;
}
