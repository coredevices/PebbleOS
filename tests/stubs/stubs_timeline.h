/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "pbl/services/timeline/item.h"
#include "apps/system/timeline/timeline.h"
#include "pbl/kernel/compiler.h"

void PBL_WEAK timeline_invoke_action(const TimelineItem *item, const TimelineItemAction *action,
                                     const AttributeList *attributes) {
}

bool PBL_WEAK timeline_add_missed_call_pin(TimelineItem *pin, uint32_t uid) {
  return true;
}

bool PBL_WEAK timeline_add(TimelineItem *item) {
  return true;
}

bool PBL_WEAK timeline_remove(const Uuid *id) {
  return true;
}

bool PBL_WEAK timeline_exists(Uuid *id) {
  return true;
}

void PBL_WEAK timeline_action_endpoint_invoke_action(const Uuid *id, uint8_t action_id,
                                                     AttributeList *attributes) {
}

Animation *PBL_WEAK timeline_animate_back_from_card(void) {
  return NULL;
}

bool PBL_WEAK timeline_get_originator_id(const TimelineItem *item, Uuid *id) {
  return false;
}
