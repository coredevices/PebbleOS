/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PIN_LOCK_MIN_LEN 4u
#define PIN_LOCK_MAX_LEN 8u
#define PIN_LOCK_SALT_LEN 16u
#define PIN_LOCK_HASH_LEN 32u  // SHA256_DIGEST_SIZE

//! Compute hash = SHA256(salt || digits). `digits` is an array of values 0-9,
//! length `len` (4-8). Output buffer must be PIN_LOCK_HASH_LEN bytes.
void pin_lock_compute_hash(const uint8_t *salt, const uint8_t *digits, uint8_t len,
                           uint8_t out_hash[PIN_LOCK_HASH_LEN]);

//! Constant-time compare of two PIN_LOCK_HASH_LEN buffers. Returns true if equal.
bool pin_lock_hash_equal(const uint8_t *a, const uint8_t *b);

//! Independent lock triggers. Each is set separately by the user.
typedef struct {
  bool enabled;               //!< feature active (a PIN is set)
  bool trigger_boot;          //!< lock at startup
  bool trigger_timeout;       //!< lock after inactivity
  bool trigger_bt_disconnect; //!< lock on phone disconnect
#ifdef CONFIG_HRM
  bool trigger_wrist_off;     //!< lock when removed from wrist (no heart rate)
#endif
  uint16_t timeout_s;         //!< inactivity seconds (0 == no auto-lock)
  bool hide_notifications;    //!< suppress notification popups when locked
  bool hide_timeline;         //!< suppress timeline peeks when locked
  bool mask_digits;           //!< confirmed digits shown as '*' (default true)
  bool haptic;                //!< light vibe on digit-flip settle (default true)
  uint8_t pin_len;            //!< 4-8
} PinLockConfig;

//! Load config from storage. Returns true on success; on missing file/record
//! all fields are zeroed (safe defaults: feature disabled).
bool pin_lock_storage_load(PinLockConfig *out);

//! Persist toggles, timeout, and pin_len. Does NOT touch hash/salt.
void pin_lock_storage_save_config(const PinLockConfig *config);

//! Generate a fresh random salt, compute and store salt+hash, set pin_len,
//! and set enabled=true. Existing toggle/timeout fields are preserved.
void pin_lock_storage_set_pin(const uint8_t *digits, uint8_t len);

//! Verify supplied digits against the stored hash. Returns false if no PIN set.
bool pin_lock_storage_verify_pin(const uint8_t *digits, uint8_t len);

//! Wipe salt+hash from storage and set enabled=false.
void pin_lock_storage_clear(void);

//! Initialize the service: load config, set initial locked state
//! (locked if enabled && trigger_boot). Call once at boot.
void pin_lock_init(void);

//! True when the watch is currently locked (navigation must be gated).
bool pin_lock_is_locked(void);

//! Force the watch into the locked state (used by "Lock now" and triggers).
//! No-op if the feature is disabled.
void pin_lock_lock_now(void);

//! Called by the unlock UI after a correct PIN: clears the locked state.
void pin_lock_mark_unlocked(void);

//! Re-read config from storage into the running service (call after Settings
//! changes the config or PIN).
void pin_lock_reload_config(void);

//! Accessors used by suppression gates and the unlock UI.
bool pin_lock_should_hide_notifications(void);
bool pin_lock_should_hide_timeline(void);
//! True when confirmed PIN digits should be masked as '*' (default true).
bool pin_lock_should_mask_digits(void);
//! True when the digit-flip haptic tick is enabled (default true).
bool pin_lock_should_haptic(void);
uint8_t pin_lock_get_pin_len(void);

//! Notify the service of user activity (resets the inactivity timer). Exposed
//! for testing; in production it's driven by button events.
void pin_lock_handle_activity(void);

//! Invoked when the inactivity timer expires (locks if the timeout trigger is
//! on). Exposed for testing; in production it's the timer callback.
void pin_lock_handle_inactivity_timeout(void);

//! Invoked on Bluetooth disconnect (locks if that trigger is on). Exposed for
//! testing; in production it's the connection-event handler.
void pin_lock_handle_bt_disconnected(void);
