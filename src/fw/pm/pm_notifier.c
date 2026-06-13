/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

/**
 * @file pm_notify.c
 * @brief PM Notifier Implementation
 */

#include "pm/pm_notifier.h"
#include "system/logging.h"

//! Maximum number of registered callbacks
#define MAX_NOTIFY_ENTRIES 32

PBL_LOG_MODULE_DEFINE(pm, CONFIG_PM_LOG_LEVEL);

//! PM notification event names
const char *const pm_notify_event_names[PM_NOTIFY_COUNT] = {
  "suspend", "resume", "shutdown", "pre_suspend", "pre_resume"
};

//! Notification entries (array-based for simplicity)
static PmNotifyEntry *s_entries[MAX_NOTIFY_ENTRIES];
static uint32_t s_entry_count = 0;
static bool s_initialized = false;

void pm_notify_init(void) {
  memset(s_entries, 0, sizeof(s_entries));
  s_entry_count = 0;
  s_initialized = true;
  PBL_LOG_INFO("PM notifier initialized");
}

bool pm_notifier_register(PmNotifyEntry *entry) {
  if (!s_initialized) {
    PBL_LOG_WRN("PM notifier not initialized");
    return false;
  }

  if (!entry || !entry->callback) {
    PBL_LOG_WRN("PM notify: invalid entry");
    return false;
  }

  if (s_entry_count >= MAX_NOTIFY_ENTRIES) {
    PBL_LOG_WRN("PM notify: callback table full");
    return false;
  }

  // Check if already registered
  for (size_t i = 0; i < s_entry_count; i++) {
    if (s_entries[i] == entry) {
      PBL_LOG_WRN("PM notify: entry already registered");
      return false;
    }
  }

  // Insert in priority order (lower priority value = higher priority)
  size_t insert_pos = s_entry_count;
  for (size_t i = 0; i < s_entry_count; i++) {
    if (entry->priority < s_entries[i]->priority) {
      insert_pos = i;
      break;
    }
  }

  // Shift entries to make room
  for (size_t i = s_entry_count; i > insert_pos; i--) {
    s_entries[i] = s_entries[i - 1];
  }

  s_entries[insert_pos] = entry;
  s_entry_count++;

  PBL_LOG_DBG("PM notify: registered callback '%s' with priority %u",
               entry->name ? entry->name : "?", entry->priority);
  return true;
}

void pm_notify_unregister(PmNotifyEntry *entry) {
  if (!s_initialized || !entry) {
    return;
  }

  for (size_t i = 0; i < s_entry_count; i++) {
    if (s_entries[i] == entry) {
      // Shift remaining entries down
      for (size_t j = i; j < s_entry_count - 1; j++) {
        s_entries[j] = s_entries[j + 1];
      }
      s_entries[s_entry_count - 1] = NULL;
      s_entry_count--;
      PBL_LOG_DBG("PM notify: unregistered callback '%s'",
                   entry->name ? entry->name : "?");
      return;
    }
  }
}

int pm_notify_notify(PmNotifyEvent event) {
  if (!s_initialized) {
    PBL_LOG_WRN("PM notifier not initialized");
    return -1;
  }

  if (event >= PM_NOTIFY_COUNT) {
    PBL_LOG_WRN("PM notify: invalid event %d", event);
    return -1;
  }

  //PBL_LOG_DBG("PM notify: event '%s' (%"PRIuS" callbacks)",
  //             pm_notify_event_names[event], s_entry_count);

  int ret = 0;

  // Notify all callbacks in priority order
  for (size_t i = 0; i < s_entry_count; i++) {
    PmNotifyEntry *entry = s_entries[i];
    if (!entry || !entry->callback) {
      continue;
    }

    PBL_LOG_DBG("PM notify: calling '%s' for event '%s'",
                 entry->name ? entry->name : "?",
                 pm_notify_event_names[event]);

    int callback_ret = entry->callback(event, entry->user_data);
    if (callback_ret < 0) {
      PBL_LOG_WRN("PM notify: callback '%s' returned error %d",
                   entry->name ? entry->name : "?", callback_ret);
      ret = callback_ret;
    }
  }

  return ret;
}

bool pm_notify_is_registered(PmNotifyEntry *entry) {
  if (!s_initialized || !entry) {
    return false;
  }

  for (size_t i = 0; i < s_entry_count; i++) {
    if (s_entries[i] == entry) {
      return true;
    }
  }

  return false;
}

uint32_t pm_notify_get_num_callbacks(void) {
  return s_entry_count;
}
