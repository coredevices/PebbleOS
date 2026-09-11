/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/events.h"

#ifdef CONFIG_SPEAKER
// Full volume = 0 dB DAC gain, the loudest undistorted level; the user's
// global speaker-volume preference scales it down from there.
#define ALARM_SPEAKER_VOLUME 100
#endif

void alarm_popup_push_window(PebbleAlarmClockEvent *e);
