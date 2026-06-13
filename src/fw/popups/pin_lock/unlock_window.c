/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "unlock_window.h"

// ── entry-buffer ──────────────────────────────────────────────────────────────

void pin_entry_init(PinEntry *e, uint8_t len) {
  *e = (PinEntry){ .len = len, .pos = 0 };
}

void pin_entry_up(PinEntry *e) {
  e->digits[e->pos] = (e->digits[e->pos] + 1) % 10;
}

void pin_entry_down(PinEntry *e) {
  e->digits[e->pos] = (e->digits[e->pos] + 9) % 10;
}

bool pin_entry_select(PinEntry *e) {
  if (e->pos + 1 >= e->len) {
    return true;
  }
  e->pos++;
  return false;
}

bool pin_entry_back(PinEntry *e) {
  if (e->pos == 0) {
    return true;
  }
  e->pos--;
  return false;
}

// Modal window UI is implemented in commit B.
void pin_unlock_window_push(void) { }
