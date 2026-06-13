/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "services/pin_lock/pin_lock.h"

//! Pure entry-buffer model, separated for unit testing.
typedef struct {
  uint8_t len;                         // number of positions (4-8)
  uint8_t pos;                         // active position
  uint8_t digits[PIN_LOCK_MAX_LEN];    // current values 0-9
} PinEntry;

void pin_entry_init(PinEntry *e, uint8_t len);
void pin_entry_up(PinEntry *e);        // active digit +1 mod 10
void pin_entry_down(PinEntry *e);      // active digit -1 mod 10
//! Confirm active digit; advance. Returns true when the last digit was just
//! confirmed (caller should verify the PIN).
bool pin_entry_select(PinEntry *e);
//! Step back one position. Returns true if we were already at the first
//! position (caller should cancel and dismiss).
bool pin_entry_back(PinEntry *e);

//! Push the unlock modal. On correct PIN it calls pin_lock_mark_unlocked()
//! and pops itself; on cancel it pops itself leaving the watch locked.
void pin_unlock_window_push(void);
