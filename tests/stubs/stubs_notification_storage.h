/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "services/normal/notifications/notification_storage.h"
#include "util/attributes.h"

void WEAK notification_storage_init(void) {
}

void WEAK notification_storage_lock(void) {
}

void WEAK notification_storage_unlock(void) {
}

void WEAK notification_storage_store(TimelineItem* notification) {
}

bool WEAK notification_storage_notification_exists(const Uuid *id) {
  return false;
}

size_t WEAK notification_storage_get_len(const Uuid *uuid) {
  return 0;
}

bool WEAK notification_storage_get(const Uuid *id, TimelineItem *item_out) {
  return false;
}

void WEAK notification_storage_set_status(const Uuid *id, uint8_t status) {
}

bool WEAK notification_storage_get_status(const Uuid *id, uint8_t *status) {
  return false;
}

void WEAK notification_storage_remove(const Uuid *id) {
}

bool WEAK notification_storage_find_ancs_notification_id(uint32_t ancs_uid, Uuid *uuid_out) {
  return false;
}

bool WEAK notification_storage_find_ancs_notification_by_timestamp(
    TimelineItem *notification, CommonTimelineItemHeader *header_out) {
  return false;
}

void WEAK notification_storage_rewrite(void (*iter_callback)(TimelineItem *notification,
    SerializedTimelineItemHeader *header, void *data), void *data) {
}

void WEAK notification_storage_iterate(
    bool (*iter_callback)(void *data, SerializedTimelineItemHeader *header_id), void *data) {}
