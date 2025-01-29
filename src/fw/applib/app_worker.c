/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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

