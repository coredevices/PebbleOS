/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "syscall/syscall.h"
#include "app_worker.h"


// ---------------------------------------------------------------------------------------------------------------
// Determine if the worker for the current app is running
bool app_worker_is_running(void) {
  return sys_app_worker_is_running();
}


// ---------------------------------------------------------------------------------------------------------------
// Launch the worker for the current app
AppWorkerResult app_worker_launch(void) {
  return sys_app_worker_launch();
}


// ---------------------------------------------------------------------------------------------------------------
// Kill the worker for the current app
AppWorkerResult app_worker_kill(void) {
  return sys_app_worker_kill();
}


// ---------------------------------------------------------------------------------------------------------------
// Subscribe to the app_message service
bool app_worker_message_subscribe(AppWorkerMessageHandler handler) {
  return plugin_service_subscribe(NULL, (PluginServiceHandler)(void *)handler);
}


// ---------------------------------------------------------------------------------------------------------------
// Unsubscribe from a specific plug-in service by uuid.
bool app_worker_message_unsubscribe(void) {
  return plugin_service_unsubscribe(NULL);
}


// ---------------------------------------------------------------------------------------------------------------
// Send an event to all registered subscribers of the given plugin service identified by UUID.
void app_worker_send_message(uint8_t type, AppWorkerMessage *data) {
  _Static_assert(sizeof(AppWorkerMessage) == sizeof(PluginEventData), "These must match!");
  plugin_service_send_event(NULL, type, (PluginEventData *)data);
}

