/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */
#include "clar.h"
#include "services/pin_lock/pin_lock.h"

#include <string.h>

#include "stubs_logging.h"
#include "stubs_passert.h"

void test_pin_lock__initialize(void) {}
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
