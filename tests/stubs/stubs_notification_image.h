/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "pbl/services/notifications/notification_image.h"
#include "pbl/kernel/compiler.h"

void PBL_WEAK notification_image_service_init(void) {
}

bool PBL_WEAK notification_image_claim(const Uuid *item_id, uint8_t *token_out) {
  return false;
}

const struct GBitmap *PBL_WEAK notification_image_lock(const Uuid *item_id) {
  return NULL;
}

void PBL_WEAK notification_image_unlock(void) {
}

bool PBL_WEAK notification_image_is_pending(const Uuid *item_id) {
  return false;
}

bool PBL_WEAK notification_image_store(uint8_t token, struct GBitmap *bitmap) {
  return false;
}

void PBL_WEAK notification_image_clear(void) {
}
