/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

// Stand-in for the firmware header, with what ctl/zbt_ctl.c uses.
#pragma once
#include <stdbool.h>
#include <stdint.h>
typedef void (*NewTimerCallback)(void *data);
typedef uint32_t TimerID;
#define TIMER_INVALID_ID           0
#define TIMER_START_FLAG_REPEATING 0x01
TimerID new_timer_create(void);
bool new_timer_start(TimerID timer, uint32_t timeout_ms, NewTimerCallback cb, void *cb_data,
                     uint32_t flags);
bool new_timer_stop(TimerID timer);
