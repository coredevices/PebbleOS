/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <string.h>

#include <pbl/fs/fs.h>
#include <pbl/i18n/i18n.h>
#include <pbl/logging/logging.h>
#include <pbl/services/language_pack.h>

#include <kernel/event_loop.h>
#include <kernel/events.h>
#include <resource/language_pack.h>
#include <resource/resource.h>
#include <resource/resource_ids.auto.h>
#include <shell/normal/language_ui.h>
#include <shell/prefs.h>

PBL_LOG_MODULE_DEFINE(service_language_pack, CONFIG_SERVICE_LANGUAGE_PACK_LOG_LEVEL);

static uint32_t s_resource_id;
static ResourceCallbackHandle s_watch_handle;
static ResourceVersion s_version;
static bool s_translating;

// Pack slot a PutBytes transfer holds open, or LANGUAGE_PACK_COUNT
static uint8_t s_install_slot = LANGUAGE_PACK_COUNT;
static bool s_select_on_install;

static int prv_read(void *data, uint32_t offset, void *buf, size_t length) {
  const uint32_t resource_id = (uintptr_t)data;
  if (resource_load_byte_range_system(SYSTEM_APP, resource_id, offset, buf, length) != length) {
    return -EIO;
  }
  return 0;
}

static void prv_unset_catalog(void) {
  pbl_i18n_set_catalog(NULL);
  s_translating = false;
}

static void prv_set_catalog(void) {
  PBL_LOG_DBG("New language detected!");
  s_version = resource_get_version(SYSTEM_APP, s_resource_id);
  if (!resource_is_valid(SYSTEM_APP, s_resource_id)) {
    prv_unset_catalog();
    return;
  }
  const struct pbl_i18n_catalog catalog = {
    .read = prv_read,
    .data = (void *)(uintptr_t)s_resource_id,
    .size = resource_size(SYSTEM_APP, s_resource_id),
  };
  s_translating = pbl_i18n_set_catalog(&catalog) == 0;
}

static uint32_t prv_language_id(void) {
  return s_translating ? s_version.crc : 0;
}

static void prv_put_event_if_changed(uint32_t prev_language_id) {
  if (prv_language_id() == prev_language_id) {
    return;
  }

  PebbleEvent event = {
    .type = PEBBLE_LANGUAGE_CHANGE_EVENT,
  };
  event_put(&event);
}

static void prv_resource_changed_handler(void *data) {
  PBL_LOG_DBG("lang resource file reloading");
  const uint32_t prev_language_id = prv_language_id();
  if (!shell_prefs_get_language_english()) {
    prv_set_catalog();
  }
  prv_put_event_if_changed(prev_language_id);
}

static void prv_resource_changed_callback(void *data) {
  // The PFS lock is still held here, so reload from an event callback instead.
  PBL_LOG_DBG("lang resource file was modified");
  launcher_task_add_callback(prv_resource_changed_handler, NULL);
}

// Language changes set the catalog, so they run on the launcher task like its readers.
static void prv_deselect_pack_cb(void *data) {
  const uint8_t slot = (uintptr_t)data;
  // Even with English toggled on: toggling it off would select whatever the slot holds by then.
  if (shell_prefs_get_language() == shell_language_for_pack_slot(slot)) {
    shell_prefs_set_language(ShellLanguageEnglish);
  }
}

static void prv_deselect_pack(uint8_t slot) {
  launcher_task_add_callback(prv_deselect_pack_cb, (void *)(uintptr_t)slot);
}

static void prv_select_installed_pack_cb(void *data) {
  shell_prefs_set_language(ShellLanguageInstalledPack);
  if (s_translating) {
    language_ui_display_changed(pbl_i18n_get_lang_name());
  }
}

void language_pack_set_resource(uint32_t resource_id) {
  // Remove prior watch, if any
  // Warning: you better be sure we're not calling from the resource changed callback.
  if (s_watch_handle) {
    resource_unwatch(s_watch_handle);
  }

  s_resource_id = resource_id;
  s_watch_handle = resource_watch(SYSTEM_APP, resource_id, prv_resource_changed_callback, NULL);

  const uint32_t prev_language_id = prv_language_id();
  if (shell_prefs_get_language_english()) {
    prv_unset_catalog();
  } else {
    prv_set_catalog();
  }
  prv_put_event_if_changed(prev_language_id);
}

void language_pack_enable(bool enable) {
  const uint32_t prev_language_id = prv_language_id();
  if (enable) {
    prv_set_catalog();
  } else {
    prv_unset_catalog();
  }
  prv_put_event_if_changed(prev_language_id);
}

bool language_pack_get_info(uint8_t slot, struct pbl_i18n_catalog_info *info) {
  if (slot >= LANGUAGE_PACK_COUNT || !info) {
    return false;
  }
  // The CRC is not checked: only the catalog's first entry is read.
  const struct pbl_i18n_catalog catalog = {
    .read = prv_read,
    .data = (void *)(uintptr_t)language_pack_resource_id(slot, RESOURCE_ID_STRINGS,
                                                         RESOURCE_ID_STRINGS),
  };
  return pbl_i18n_catalog_get_info(&catalog, info) == 0;
}

bool language_pack_has_fonts(uint8_t slot) {
  for (uint32_t font = RESOURCE_ID_GOTHIC_14_EXTENDED;
       font <= RESOURCE_ID_LECO_60_BOLD_NUMBERS_AM_PM_EXTENDED; ++font) {
    if (resource_is_valid(SYSTEM_APP, language_pack_resource_id(slot, font, RESOURCE_ID_STRINGS))) {
      return true;
    }
  }
  return false;
}

bool language_pack_remove(uint8_t slot) {
  const char *filename = language_pack_filename(slot);
  if (!filename) {
    return false;
  }
  // PFS croaks when deleting a file that a transfer still holds open
  if (slot == s_install_slot || pbl_fs_unlink(filename) != 0) {
    return false;
  }
  prv_deselect_pack(slot);
  return true;
}

const char *language_pack_install_begin(const char *filename) {
  s_select_on_install = false;
  s_install_slot = LANGUAGE_PACK_COUNT;
  for (uint8_t slot = 0; slot < LANGUAGE_PACK_COUNT; ++slot) {
    if (strcmp(filename, language_pack_filename(slot)) == 0) {
      // Legacy transfers replace slot zero and select it once complete.
      s_select_on_install = slot == 0;
      s_install_slot = slot;
      return filename;
    }
  }
  if (strcmp(filename, LANGUAGE_PACK_ADD_FILENAME) != 0) {
    return filename;
  }
  for (uint8_t slot = 0; slot < LANGUAGE_PACK_COUNT; ++slot) {
    if (language_pack_is_installed(slot)) {
      continue;
    }
    // The empty slot may still be selected, e.g. slot zero on a fresh watch.
    prv_deselect_pack(slot);
    s_install_slot = slot;
    return language_pack_filename(slot);
  }
  return NULL;
}

void language_pack_install_end(bool success) {
  if (success && s_select_on_install) {
    launcher_task_add_callback(prv_select_installed_pack_cb, NULL);
  }
  s_select_on_install = false;
  s_install_slot = LANGUAGE_PACK_COUNT;
}
