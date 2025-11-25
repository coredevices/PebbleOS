/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>

typedef void * RemoteRef;

RemoteRef remote_get_active() {
  return NULL;
}

bool remote_is_connected(void) {
  return false;
}
