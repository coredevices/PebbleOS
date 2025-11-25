/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <clar.h>

#include "kernel/pebble_tasks.h"
#include "util/rand.h"

// Stubs
///////////////////////////////////////////////////////////
#include "stubs_passert.h"
#include "stubs_rand_ptr.h"

PebbleTask pebble_task_get_current(void) {
  return PebbleTask_KernelMain; // System seed
}

// Tests
///////////////////////////////////////////////////////////

void test_rand__smoke_test(void) {
#define RANDOM_CHECK_LENGTH 512

  uint32_t values[RANDOM_CHECK_LENGTH];
  for(size_t i = 0; i < RANDOM_CHECK_LENGTH; i++) {
    values[i] = rand32();
    for(size_t l = 0; l < i; l++) {
      printf("i,l: %zu,%zu\n", i,l);
      cl_assert(values[i] != values[l]);
    }
  }
}
