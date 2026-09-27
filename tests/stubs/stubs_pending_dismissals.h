/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "pbl/services/notifications/pending_dismissals.h"
#include "pbl/kernel/compiler.h"

bool PBL_WEAK pending_dismissals_add(const TimelineItem *notification,
                                     const TimelineItemAction *dismiss) {
  return false;
}
