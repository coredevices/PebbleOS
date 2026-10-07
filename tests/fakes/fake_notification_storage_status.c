/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

//! Records notification_storage_set_status() calls. It is a source file of its own, so it takes
//! the place of the PBL_WEAK version in stubs_notification_storage.h at link time.

#include "fake_notification_storage_status.h"

#include "pbl/services/notifications/notification_storage.h"

#include <string.h>

static bool s_was_set;
static Uuid s_last_id;
static uint8_t s_last_status;

void fake_notification_storage_status_reset(void) {
  s_was_set = false;
  s_last_id = UUID_INVALID;
  s_last_status = 0;
}

bool fake_notification_storage_status_was_set(void) {
  return s_was_set;
}

uint8_t fake_notification_storage_status_get_last(Uuid *id_out) {
  *id_out = s_last_id;
  return s_last_status;
}

// Flash can only clear bits and status is stored inverted, so writes to one id add bits up
void notification_storage_set_status(const Uuid *id, uint8_t status) {
  if (!s_was_set || memcmp(id, &s_last_id, sizeof(Uuid)) != 0) {
    s_last_status = 0;
  }
  s_was_set = true;
  s_last_id = *id;
  s_last_status |= status;
}
