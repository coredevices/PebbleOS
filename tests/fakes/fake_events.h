/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/events.h"

void fake_event_init(void);

PebbleEvent fake_event_get_last(void);

void fake_event_clear_last(void);

void fake_event_reset_count(void);

uint32_t fake_event_get_count(void);

void **fake_event_get_buffer(PebbleEvent *event);

typedef void (*FakeEventCallback)(PebbleEvent *event);
void fake_event_set_callback(FakeEventCallback cb);

