/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "grenade_launcher.h"

#include "applib/app.h"
#include "applib/fonts/fonts.h"
#include "applib/ui/ui.h"
#include "applib/ui/window.h"
#include "applib/ui/window_private.h"
#include "drivers/flash.h"
#include "drivers/system_flash.h"
#include "flash_region/flash_region.h"
#include "kernel/pbl_malloc.h"
#include "process_state/app_state/app_state.h"
#include "services/common/system_task.h"
#include "services/runlevel.h"
#include "system/bootbits.h"
#include "system/firmware_storage.h"
#include "system/logging.h"
#include "system/passert.h"
#include "system/reset.h"

#include <stdio.h>

///////////
// Helpers

static void fw_update_reboot(void) {
  PBL_LOG(LOG_LEVEL_DEBUG, "Rebooting to apply new firmware!");

  boot_bit_set(BOOT_BIT_NEW_FW_AVAILABLE);

  services_set_runlevel(RunLevel_BareMinimum);
  system_reset();
}

static void erase_fw(const uint32_t start_address) {
  // Erase flash
  flash_erase_sector_blocking(start_address); // Set everything high

  // Set up the firmware description
  FirmwareDescription desc;
  desc.description_length = sizeof(FirmwareDescription);
  desc.firmware_length = (64 * 1024) - sizeof(FirmwareDescription);
  desc.checksum = 0xdeadbeef;

  // Write the firmware description
  flash_write_bytes((uint8_t*)&desc, start_address, desc.description_length);
}

enum {
  ERASE_NORMAL_FW = 1 << 0,
  ERASE_RECOVERY_FW = 1 << 1,
  ERASE_ALL = ERASE_NORMAL_FW | ERASE_RECOVERY_FW,
};

static void erase_callback(void *data) {
  uintptr_t arg = (uintptr_t)data;

  if (arg & ERASE_RECOVERY_FW) {
    erase_fw(FLASH_REGION_SAFE_FIRMWARE_BEGIN);
  }

  if (arg & ERASE_NORMAL_FW) {
    erase_fw(FLASH_REGION_FIRMWARE_DEST_BEGIN);

    system_flash_erase(FLASH_Sector_4);
    system_flash_erase(FLASH_Sector_5);
    system_flash_erase(FLASH_Sector_6);
    system_flash_erase(FLASH_Sector_7);
  }

  fw_update_reboot();
}

static void crash(void *data) {
  ((void(*)(void))data)();
}

////////////////////
// UI Code

typedef struct {
  Window window;
  TextLayer text;
} AppData;

static void set_text(Window *window, char *message) {
  AppData *data = window_get_user_data(window);
  TextLayer *text = &data->text;
  text_layer_set_text(text, message);
  PBL_LOG(LOG_LEVEL_DEBUG, "%s", message);
}

static void up_click_handler(ClickRecognizerRef recognizer, Window *window) {
  (void) recognizer;
  set_text(window, "Erasing Normal+Sys firmware...");
  system_task_add_callback(erase_callback, (void *)ERASE_NORMAL_FW);
}

#if 0
static void up_long_click_handler(ClickRecognizerRef recognizer, Window *window) {
  (void) recognizer;
  set_text(window, "Erasing All...");
  system_task_add_callback(erase_callback, (void *)ERASE_ALL);
}
#endif

static void select_click_handler(ClickRecognizerRef recognizer, Window *window) {
  (void) recognizer;
  set_text(window, "Going down for reboot...");
  system_task_add_callback(system_reset_callback, NULL);
}

static void down_click_handler(ClickRecognizerRef recognizer, Window *window) {
  (void) recognizer;
  set_text(window, "Erasing recovery firmware");
  system_task_add_callback(erase_callback, (void *)ERASE_RECOVERY_FW);
}

static void back_click_handler(ClickRecognizerRef recognizer, Window *window) {
  (void) recognizer;
  set_text(window, "Boom!");
  system_task_add_callback(crash, NULL);
}

static void config_provider(Window *window) {
  window_single_click_subscribe(BUTTON_ID_UP, (ClickHandler) up_click_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, (ClickHandler) select_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, (ClickHandler) down_click_handler);
  window_single_click_subscribe(BUTTON_ID_BACK, (ClickHandler) back_click_handler);
}

static void prv_window_load(Window *window) {
  AppData *data = window_get_user_data(window);
  TextLayer *text = &data->text;
  text_layer_init(text, &window->layer.bounds);
  text_layer_set_text(text, "UP: Erase Normal+Sys FW\nUP LONG:Erase Normal+Recov+Sys\nSEL: Reboot FW\nDOWN: Erase Recovery\nBACK: Crash");
  text_layer_set_font(text, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  layer_add_child(&window->layer, &text->layer);
}

static void push_window(AppData *data) {
  Window *window = &data->window;
  window_init(window, WINDOW_NAME("Grenade Launcher"));
  window_set_user_data(window, data);
  window_set_overrides_back_button(window, true);
  window_set_window_handlers(window, &(WindowHandlers) {
    .load = prv_window_load,
  });
  window_set_click_config_provider(window, (ClickConfigProvider) config_provider);
  window_set_fullscreen(window, true);
  const bool animated = false;
  app_window_stack_push(window, animated);
}

////////////////////
// App boilerplate

static void handle_init(void) {
  AppData *data = app_malloc_check(sizeof(AppData));

  app_state_set_user_data(data);
  push_window(data);
}

static void handle_deinit(void) {
  AppData *data = app_state_get_user_data();
  app_free(data);
}

static void s_main(void) {
  handle_init();

  app_event_loop();

  handle_deinit();
}

const PebbleProcessMd* grenade_launcher_app_get_info() {
  static const PebbleProcessMdSystem s_app_info = {
    .common.main_func = s_main,
    .name = "Grenade Launcher"
  };
  return (const PebbleProcessMd*) &s_app_info;
}

