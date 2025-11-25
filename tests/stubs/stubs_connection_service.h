/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "applib/connection_service.h"
#include "applib/connection_service_private.h"

bool connection_service_peek_pebble_app_connection(void) {
  return false;
}

bool connection_service_peek_pebblekit_connection(void) {
  return false;
}

void connection_service_unsubscribe(void) {
}

void connection_service_subscribe(ConnectionHandlers conn_handlers) {
}

void connection_service_state_init(ConnectionServiceState *state) {
}

void bluetooth_connection_service_subscribe(ConnectionHandler handler) {
}

void bluetooth_connection_service_unsubscribe(void) {
}

bool bluetooth_connection_service_peek(void) {
  return false;
}
