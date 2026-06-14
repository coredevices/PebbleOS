/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "security.h"
#include "menu.h"
#include "option_menu.h"
#include "security_pin_entry.h"
#include "window.h"

#include "applib/ui/app_window_stack.h"
#include "applib/ui/ui.h"
#include "kernel/pbl_malloc.h"
#include "pbl/services/i18n/i18n.h"
#include "services/pin_lock/pin_lock.h"
#include "syscall/syscall.h"
#include "system/passert.h"
#include "util/size.h"

typedef enum {
  ROW_ENABLED = 0,
  ROW_CHANGE_PIN,
  ROW_TRIGGER_BOOT,
  ROW_TRIGGER_TIMEOUT,
  ROW_TRIGGER_BT,
  ROW_HIDE_NOTIFS,
  ROW_HIDE_TIMELINE,
  ROW_SHOW_DIGITS,
  ROW_HAPTIC,
  ROW_LOCK_NOW,
  NUM_ROWS_ALL,
} SecurityRow;

typedef struct SecurityData {
  SettingsCallbacks callbacks;
  PinLockConfig cfg;
} SecurityData;

// Auto-lock interval picker. Index 0 is "Off" (the timeout trigger disabled);
// the rest enable it with the matching inactivity duration.
static const uint16_t s_timeout_values[] = {
  0, 0, 30, 60, 180, 300, 600, 900, 1800, 3600,
};
static const bool s_timeout_on[] = {
  false, true, true, true, true, true, true, true, true, true,
};
static const char *s_timeout_labels[] = {
  i18n_noop("Off"),
  i18n_noop("Immediately"),
  i18n_noop("30 Seconds"),
  i18n_noop("1 Minute"),
  i18n_noop("3 Minutes"),
  i18n_noop("5 Minutes"),
  i18n_noop("10 Minutes"),
  i18n_noop("15 Minutes"),
  i18n_noop("30 Minutes"),
  i18n_noop("1 Hour"),
};

static int prv_timeout_index(const SecurityData *data) {
  if (!data->cfg.trigger_timeout) {
    return 0;  // Off
  }
  for (size_t i = 1; i < ARRAY_LENGTH(s_timeout_values); i++) {
    if (s_timeout_values[i] == data->cfg.timeout_s) {
      return (int)i;
    }
  }
  return 1;  // a stored duration we don't list -> default to Immediately
}

static void prv_timeout_select(OptionMenu *option_menu, int selection, void *context) {
  SecurityData *data = (SecurityData *)context;
  data->cfg.trigger_timeout = s_timeout_on[selection];
  data->cfg.timeout_s = s_timeout_values[selection];
  pin_lock_storage_save_config(&data->cfg);
  sys_pin_lock_reload_config();  // apply to the live kernel-side state
  pin_lock_storage_load(&data->cfg);
  // Pop the picker; the Security submenu refreshes via its appear callback.
  app_window_stack_remove(&option_menu->window, true /* animated */);
}

static void prv_timeout_menu_push(SecurityData *data) {
  const int index = prv_timeout_index(data);
  const OptionMenuCallbacks callbacks = {
    .select = prv_timeout_select,
  };
  settings_option_menu_push(i18n_noop("Auto-Lock"), OptionMenuContentType_SingleLine,
                            index, &callbacks,
                            ARRAY_LENGTH(s_timeout_labels), false /* icons */,
                            s_timeout_labels, data);
}

// Helpers
static void prv_reload_cfg(SecurityData *data) {
  pin_lock_storage_load(&data->cfg);
}

static void prv_save_and_reload(SecurityData *data) {
  pin_lock_storage_save_config(&data->cfg);
  sys_pin_lock_reload_config();  // apply to the live kernel-side state
  prv_reload_cfg(data);
}

// SettingsCallbacks

static void prv_deinit_cb(SettingsCallbacks *context) {
  SecurityData *data = (SecurityData *)context;
  i18n_free_all(data);
  app_free(data);
}

static uint16_t prv_num_rows_cb(SettingsCallbacks *context) {
  SecurityData *data = (SecurityData *)context;
  // When disabled show only the Enable row.
  return data->cfg.enabled ? NUM_ROWS_ALL : 1;
}

static void prv_draw_row_cb(SettingsCallbacks *context, GContext *ctx,
                            const Layer *cell_layer, uint16_t row, bool selected) {
  SecurityData *data = (SecurityData *)context;
  const char *title = NULL;
  const char *subtitle = NULL;

  switch ((SecurityRow)row) {
    case ROW_ENABLED:
      title = i18n_noop("PIN Lock");
      subtitle = data->cfg.enabled ? i18n_noop("On") : i18n_noop("Off");
      break;
    case ROW_CHANGE_PIN:
      title = i18n_noop("Change PIN");
      break;
    case ROW_TRIGGER_BOOT:
      title = i18n_noop("Lock on Start");
      subtitle = data->cfg.trigger_boot ? i18n_noop("On") : i18n_noop("Off");
      break;
    case ROW_TRIGGER_TIMEOUT:
      title = i18n_noop("Auto-Lock");
      subtitle = s_timeout_labels[prv_timeout_index(data)];
      break;
    case ROW_TRIGGER_BT:
      title = i18n_noop("Lock on BT Disconnect");
      subtitle = data->cfg.trigger_bt_disconnect ? i18n_noop("On") : i18n_noop("Off");
      break;
    case ROW_HIDE_NOTIFS:
      title = i18n_noop("Hide Notifications");
      subtitle = data->cfg.hide_notifications ? i18n_noop("On") : i18n_noop("Off");
      break;
    case ROW_HIDE_TIMELINE:
      title = i18n_noop("Hide Timeline");
      subtitle = data->cfg.hide_timeline ? i18n_noop("On") : i18n_noop("Off");
      break;
    case ROW_SHOW_DIGITS:
      title = i18n_noop("Show PIN digits");
      subtitle = !data->cfg.mask_digits ? i18n_noop("On") : i18n_noop("Off");
      break;
    case ROW_HAPTIC:
      title = i18n_noop("Haptic feedback");
      subtitle = data->cfg.haptic ? i18n_noop("On") : i18n_noop("Off");
      break;
    case ROW_LOCK_NOW:
      title = i18n_noop("Lock Now");
      break;
    default:
      return;
  }

  menu_cell_basic_draw(ctx, cell_layer,
                       i18n_get(title, data),
                       subtitle ? i18n_get(subtitle, data) : NULL,
                       NULL);
}

// Completion callbacks for the PIN flow invoked from security_pin_entry.

static void prv_on_set_pin_complete(bool success, const uint8_t *digits, uint8_t len, void *ctx) {
  if (!success) {
    return;
  }
  SecurityData *data = (SecurityData *)ctx;
  pin_lock_storage_set_pin(digits, len);
  sys_pin_lock_reload_config();  // kernel picks up enabled + new pin_len
  prv_reload_cfg(data);
  settings_menu_reload_data(SettingsMenuItemSecurity);
  settings_menu_mark_dirty(SettingsMenuItemSecurity);
}

// Two-step change-PIN: first verify the current PIN, then set a new one.
static void prv_on_change_pin_set_complete(bool success, const uint8_t *digits, uint8_t len,
                                           void *ctx) {
  prv_on_set_pin_complete(success, digits, len, ctx);
}

static void prv_on_verify_for_change_complete(bool success, const uint8_t *digits, uint8_t len,
                                              void *ctx) {
  if (!success) {
    return;
  }
  SecurityData *data = (SecurityData *)ctx;
  const SecurityPinEntryConfig set_cfg = {
    .mode = SecurityPinEntryMode_Set,
    .on_complete = prv_on_change_pin_set_complete,
    .ctx = data,
  };
  security_pin_entry_push(&set_cfg);
}

static void prv_on_verify_for_disable_complete(bool success, const uint8_t *digits, uint8_t len,
                                               void *ctx) {
  if (!success) {
    return;
  }
  SecurityData *data = (SecurityData *)ctx;
  pin_lock_storage_clear();
  sys_pin_lock_reload_config();  // kernel picks up disabled state
  prv_reload_cfg(data);
  settings_menu_reload_data(SettingsMenuItemSecurity);
  settings_menu_mark_dirty(SettingsMenuItemSecurity);
}

static void prv_select_click_cb(SettingsCallbacks *context, uint16_t row) {
  SecurityData *data = (SecurityData *)context;

  switch ((SecurityRow)row) {
    case ROW_ENABLED:
      if (data->cfg.enabled) {
        // Disable: verify the current PIN first.
        const SecurityPinEntryConfig cfg = {
          .mode = SecurityPinEntryMode_Verify,
          .on_complete = prv_on_verify_for_disable_complete,
          .ctx = data,
        };
        security_pin_entry_push(&cfg);
      } else {
        // Enable: set a new PIN.
        const SecurityPinEntryConfig cfg = {
          .mode = SecurityPinEntryMode_Set,
          .on_complete = prv_on_set_pin_complete,
          .ctx = data,
        };
        security_pin_entry_push(&cfg);
      }
      return; // redraw happens in the completion callbacks
    case ROW_CHANGE_PIN:
      if (data->cfg.enabled) {
        // Verify existing PIN then set a new one.
        const SecurityPinEntryConfig cfg = {
          .mode = SecurityPinEntryMode_Verify,
          .on_complete = prv_on_verify_for_change_complete,
          .ctx = data,
        };
        security_pin_entry_push(&cfg);
      } else {
        // No PIN set yet; go straight to set-PIN flow.
        const SecurityPinEntryConfig cfg = {
          .mode = SecurityPinEntryMode_Set,
          .on_complete = prv_on_set_pin_complete,
          .ctx = data,
        };
        security_pin_entry_push(&cfg);
      }
      return;
    case ROW_TRIGGER_BOOT:
      data->cfg.trigger_boot = !data->cfg.trigger_boot;
      prv_save_and_reload(data);
      break;
    case ROW_TRIGGER_TIMEOUT:
      // Always open the picker; it includes "Off" to disable the trigger.
      prv_timeout_menu_push(data);
      return;
    case ROW_TRIGGER_BT:
      data->cfg.trigger_bt_disconnect = !data->cfg.trigger_bt_disconnect;
      prv_save_and_reload(data);
      break;
    case ROW_HIDE_NOTIFS:
      data->cfg.hide_notifications = !data->cfg.hide_notifications;
      prv_save_and_reload(data);
      break;
    case ROW_HIDE_TIMELINE:
      data->cfg.hide_timeline = !data->cfg.hide_timeline;
      prv_save_and_reload(data);
      break;
    case ROW_SHOW_DIGITS:
      data->cfg.mask_digits = !data->cfg.mask_digits;
      prv_save_and_reload(data);
      break;
    case ROW_HAPTIC:
      data->cfg.haptic = !data->cfg.haptic;
      prv_save_and_reload(data);
      break;
    case ROW_LOCK_NOW:
      sys_pin_lock_lock_now();
      return; // no redraw needed
    default:
      return;
  }

  settings_menu_reload_data(SettingsMenuItemSecurity);
  settings_menu_mark_dirty(SettingsMenuItemSecurity);
}

static void prv_appear_cb(SettingsCallbacks *context) {
  SecurityData *data = (SecurityData *)context;
  // Refresh config whenever the submenu becomes visible (e.g. after PIN flow pops).
  prv_reload_cfg(data);
  settings_menu_reload_data(SettingsMenuItemSecurity);
  settings_menu_mark_dirty(SettingsMenuItemSecurity);
}

static Window *prv_init(void) {
  SecurityData *data = app_malloc_check(sizeof(*data));
  *data = (SecurityData){};

  pin_lock_storage_load(&data->cfg);

  data->callbacks = (SettingsCallbacks){
    .deinit = prv_deinit_cb,
    .draw_row = prv_draw_row_cb,
    .select_click = prv_select_click_cb,
    .num_rows = prv_num_rows_cb,
    .appear = prv_appear_cb,
  };

  return settings_window_create(SettingsMenuItemSecurity, &data->callbacks);
}

const SettingsModuleMetadata *settings_security_get_info(void) {
  static const SettingsModuleMetadata s_module_info = {
    .name = i18n_noop("Security"),
    .init = prv_init,
  };
  return &s_module_info;
}
