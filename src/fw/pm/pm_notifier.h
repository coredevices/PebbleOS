/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

/**
 * @file pm_notify.h
 * @brief PM Notifier API (Zephyr-style)
 *
 * Provides a notification mechanism for power management events.
 * Modules can register callbacks to be notified when the system
 * is about to suspend, resume, or shutdown.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

//! @addtogroup PM_Notify PM Notifier
//! @{

//! PM notification events (aligned with Zephyr's pm_notify_event)
typedef enum {
  PM_NOTIFY_SUSPEND = 0,     //!< System is about to suspend
  PM_NOTIFY_RESUME,          //!< System is resuming
  PM_NOTIFY_SHUTDOWN,        //!< System is shutting down
  PM_NOTIFY_PRE_SUSPEND,     //!< Pre-suspend notification
  PM_NOTIFY_PRE_RESUME,      //!< Pre-resume notification
  PM_NOTIFY_COUNT
} PmNotifyEvent;

//! PM notification event names for logging
extern const char *const pm_notify_event_names[PM_NOTIFY_COUNT];

//! PM notification callback type
typedef int (*PmNotifyCallback)(PmNotifyEvent event, void *user_data);

//! PM notification priority levels
typedef enum {
  PM_NOTIFY_PRIORITY_HIGHEST = 0,
  PM_NOTIFY_PRIORITY_HIGH = 64,
  PM_NOTIFY_PRIORITY_NORMAL = 128,
  PM_NOTIFY_PRIORITY_LOW = 192,
  PM_NOTIFY_PRIORITY_LOWEST = 255,
} PmNotifyPriority;

//! PM notification entry structure
typedef struct PmNotifyEntry {
  PmNotifyCallback callback;   //!< Callback function
  void *user_data;             //!< User data passed to callback
  uint8_t priority;            //!< Priority (lower = higher priority)
  const char *name;            //!< Name for debugging
  struct PmNotifyEntry *next;  //!< Next entry in list
} PmNotifyEntry;

//! Initialize PM notification subsystem
void pm_notify_init(void);

//! Register a PM notification callback
//! @param entry Pointer to notification entry (must be static/allocated)
//! @return true if registered successfully, false on error
bool pm_notifier_register(PmNotifyEntry *entry);

//! Unregister a PM notification callback
//! @param entry Pointer to notification entry to remove
void pm_notify_unregister(PmNotifyEntry *entry);

//! Notify all registered callbacks of an event
//! @param event Event to notify
//! @return 0 on success, negative error code if any callback failed
int pm_notify_notify(PmNotifyEvent event);

//! Check if a callback is registered
//! @param entry Pointer to notification entry
//! @return true if registered
bool pm_notify_is_registered(PmNotifyEntry *entry);

//! Get number of registered callbacks
uint32_t pm_notify_get_num_callbacks(void);

//! @} PM_Notify

// =============================================================================
// Helper macros for common use cases
// =============================================================================

//! Define a PM notification callback with shutdown handler
#define PM_NOTIFY_SHUTDOWN_HANDLER(_name, _handler) \
  static int _name##_pm_notify_handler(PmNotifyEvent event, void *user_data) { \
    (void)user_data; \
    if (event == PM_NOTIFY_SHUTDOWN) { \
      _handler(); \
    } \
    return 0; \
  } \
  static PmNotifyEntry _name##_pm_notify_entry = { \
    .callback = _name##_pm_notify_handler, \
    .user_data = NULL, \
    .priority = PM_NOTIFY_PRIORITY_NORMAL, \
    .name = #_name, \
    .next = NULL, \
  };

//! Define a complete PM notification handler
#define PM_NOTIFY_HANDLER(_name, _priority) \
  static PmNotifyEntry _name##_pm_notify_entry = { \
    .callback = _name##_pm_notify_callback, \
    .user_data = NULL, \
    .priority = _priority, \
    .name = #_name, \
    .next = NULL, \
  };

//! Register a shutdown handler (convenience macro)
#define PM_NOTIFY_REGISTER_SHUTDOWN(_name) \
  pm_notify_register(&_name##_pm_notify_entry)
