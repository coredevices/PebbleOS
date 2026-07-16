/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/events.h"

#ifdef CONFIG_SPEAKER
// Volume 60/100 is a moderate first cut; tunable, and a per-user volume
// preference can be added in a follow-up.
#define ALARM_SPEAKER_VOLUME 60
#endif

void alarm_popup_push_window(PebbleAlarmClockEvent* e);
