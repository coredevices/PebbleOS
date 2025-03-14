/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

///////////////////////////////////////
// Implements:
//   int rand(void);
//   int rand_r(unsigned int *seedp);
//   void srand(unsigned int seed);
///////////////////////////////////////
// Exports to apps:
//   rand, srand
///////////////////////////////////////
// Notes:
//   RNG implementation is using TinyMT
//   Apps and Workers have unique RNG seed variables, all kernel tasks share an RNG seed.
//   All other libc's seem to just segfault if seedp is NULL to rand_r; we assert instead.

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "kernel/pebble_tasks.h"
#include "system/passert.h"
#include "tinymt32.h"

extern uint32_t *app_state_get_rand_ptr(void);
extern uint32_t *worker_state_get_rand_ptr(void);

// Kernel random seed
static tinymt32_t s_kernel_rand = {{0}};

static tinymt32_t *prv_get_seed_ptr(void) {
  switch (pebble_task_get_current()) {
    case PebbleTask_App:
      return (tinymt32_t*)app_state_get_rand_ptr();
    case PebbleTask_Worker:
      return (tinymt32_t*)worker_state_get_rand_ptr();
    default:
      return &s_kernel_rand;
  }
}

static void prv_seed(tinymt32_t *state, uint32_t seed) {
  // Generated from ID 2841590142
  // characteristic=9a1431e60e5e03b118c9173c2f60761f
  // type=32
  // id=2841590142
  // mat1=d728239b
  // mat2=57e7ffaf
  // tmat=ebb03f7f
  // weight=59
  // delta=0

  state->mat1 = 0xd728239b;
  state->mat2 = 0x57e7ffaf;
  state->tmat = 0xebb03f7f;
  tinymt32_init(state, seed);
}

static int prv_next(tinymt32_t *state) {
  if (state->mat1 == 0) { // Not initialized yet
    prv_seed(state, 0x9a1431e6); // Just any ol' number
  }
  return tinymt32_generate_uint32(state);
}

uint32_t rand32(void) {
  return prv_next(prv_get_seed_ptr());
}

int rand(void) {
  return rand32() & 0x7FFFFFFF;
}

int rand_r(unsigned int *seedp) { // Please don't use this
  PBL_ASSERTN(seedp != NULL);

  tinymt32_t state = {{0}};
  prv_seed(&state, *seedp);
  *seedp = prv_next(&state) & 0x7FFFFFFF;
  return *seedp;
}

void srand(unsigned int seed) {
  prv_seed(prv_get_seed_ptr(), seed);
}
