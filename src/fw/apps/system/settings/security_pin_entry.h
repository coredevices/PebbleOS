/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */
#pragma once

#include "services/pin_lock/pin_lock.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum {
  //! Collect a new PIN (two-phase: enter then confirm).
  SecurityPinEntryMode_Set,
  //! Verify the existing PIN against storage.
  SecurityPinEntryMode_Verify,
} SecurityPinEntryMode;

//! Completion callback invoked after the final digit is confirmed.
//! @param success  true when PIN matched (Verify) or both phases matched (Set).
//! @param digits   The confirmed digit array (only valid when success=true).
//! @param len      Number of digits.
//! @param ctx      Caller-supplied context pointer.
typedef void (*SecurityPinEntryCallback)(bool success, const uint8_t *digits, uint8_t len,
                                         void *ctx);

typedef struct {
  SecurityPinEntryMode mode;
  SecurityPinEntryCallback on_complete;
  void *ctx;
} SecurityPinEntryConfig;

//! Push an app-context PIN-entry window.
void security_pin_entry_push(const SecurityPinEntryConfig *config);
