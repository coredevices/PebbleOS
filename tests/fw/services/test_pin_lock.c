/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */
#include "clar.h"
#include "services/pin_lock/pin_lock.h"

#include <string.h>

#include "stubs_logging.h"
#include "stubs_passert.h"
#include "fake_settings_file.h"
#include "fake_rng.h"
#include "stubs_mutex.h"

void test_pin_lock__initialize(void) {
  fake_settings_file_reset();
}
void test_pin_lock__cleanup(void) {}

void test_pin_lock__hash_is_deterministic(void) {
  const uint8_t salt[PIN_LOCK_SALT_LEN] = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};
  const uint8_t digits[4] = {1,2,3,4};
  uint8_t h1[PIN_LOCK_HASH_LEN], h2[PIN_LOCK_HASH_LEN];
  pin_lock_compute_hash(salt, digits, 4, h1);
  pin_lock_compute_hash(salt, digits, 4, h2);
  cl_assert_equal_b(true, pin_lock_hash_equal(h1, h2));
}

void test_pin_lock__different_pin_different_hash(void) {
  const uint8_t salt[PIN_LOCK_SALT_LEN] = {0};
  const uint8_t a[4] = {1,2,3,4};
  const uint8_t b[4] = {1,2,3,5};
  uint8_t ha[PIN_LOCK_HASH_LEN], hb[PIN_LOCK_HASH_LEN];
  pin_lock_compute_hash(salt, a, 4, ha);
  pin_lock_compute_hash(salt, b, 4, hb);
  cl_assert_equal_b(false, pin_lock_hash_equal(ha, hb));
}

void test_pin_lock__different_salt_different_hash(void) {
  const uint8_t s1[PIN_LOCK_SALT_LEN] = {0};
  const uint8_t s2[PIN_LOCK_SALT_LEN] = {9};
  const uint8_t digits[4] = {1,2,3,4};
  uint8_t h1[PIN_LOCK_HASH_LEN], h2[PIN_LOCK_HASH_LEN];
  pin_lock_compute_hash(s1, digits, 4, h1);
  pin_lock_compute_hash(s2, digits, 4, h2);
  cl_assert_equal_b(false, pin_lock_hash_equal(h1, h2));
}

void test_pin_lock__no_pin_by_default(void) {
  PinLockConfig cfg;
  cl_assert_equal_b(true, pin_lock_storage_load(&cfg));
  cl_assert_equal_b(false, cfg.enabled);
  const uint8_t digits[4] = {1,2,3,4};
  cl_assert_equal_b(false, pin_lock_storage_verify_pin(digits, 4));
}

void test_pin_lock__set_and_verify_pin(void) {
  const uint8_t pin[6] = {9,8,7,6,5,4};
  pin_lock_storage_set_pin(pin, 6);
  PinLockConfig cfg;
  cl_assert_equal_b(true, pin_lock_storage_load(&cfg));
  cl_assert_equal_b(true, cfg.enabled);
  cl_assert_equal_i(6, cfg.pin_len);
  cl_assert_equal_b(true, pin_lock_storage_verify_pin(pin, 6));
  const uint8_t wrong[6] = {9,8,7,6,5,5};
  cl_assert_equal_b(false, pin_lock_storage_verify_pin(wrong, 6));
}

void test_pin_lock__clear_removes_pin(void) {
  const uint8_t pin[4] = {1,2,3,4};
  pin_lock_storage_set_pin(pin, 4);
  pin_lock_storage_clear();
  PinLockConfig cfg;
  pin_lock_storage_load(&cfg);
  cl_assert_equal_b(false, cfg.enabled);
  cl_assert_equal_b(false, pin_lock_storage_verify_pin(pin, 4));
}

void test_pin_lock__config_round_trips(void) {
  const uint8_t pin[4] = {1,2,3,4};
  pin_lock_storage_set_pin(pin, 4);
  PinLockConfig cfg;
  pin_lock_storage_load(&cfg);
  cfg.trigger_boot = true;
  cfg.trigger_bt_disconnect = true;
  cfg.timeout_s = 60;
  cfg.hide_notifications = true;
  pin_lock_storage_save_config(&cfg);
  PinLockConfig got;
  pin_lock_storage_load(&got);
  cl_assert_equal_b(true, got.trigger_boot);
  cl_assert_equal_b(true, got.trigger_bt_disconnect);
  cl_assert_equal_i(60, got.timeout_s);
  cl_assert_equal_b(true, got.hide_notifications);
  cl_assert_equal_b(true, got.enabled);
}
