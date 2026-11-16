/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <pbl/profiler/profiler.h>

#define VALUES 128U
#define ROUNDS 20U

/* Each node accumulates the cycles spent between its start and its stop. */
PBL_PROFILER_NODE_DEFINE(bubble_sort);
PBL_PROFILER_NODE_DEFINE(insertion_sort);
PBL_PROFILER_NODE_DEFINE(shuffle);

static uint16_t s_values[VALUES];

static void prv_shuffle(uint32_t seed) {
  for (unsigned int i = 0; i < VALUES; i++) {
    seed = seed * 1664525U + 1013904223U;
    s_values[i] = (uint16_t)(seed >> 16);
  }
}

static void prv_bubble_sort(void) {
  for (unsigned int i = 0; i < VALUES - 1U; i++) {
    for (unsigned int j = 0; j < VALUES - 1U - i; j++) {
      if (s_values[j] > s_values[j + 1U]) {
        uint16_t tmp = s_values[j];

        s_values[j] = s_values[j + 1U];
        s_values[j + 1U] = tmp;
      }
    }
  }
}

static void prv_insertion_sort(void) {
  for (unsigned int i = 1; i < VALUES; i++) {
    uint16_t val = s_values[i];
    unsigned int j = i;

    while (j > 0 && s_values[j - 1U] > val) {
      s_values[j] = s_values[j - 1U];
      j--;
    }
    s_values[j] = val;
  }
}

int main(void) {
  pbl_profiler_reset();
  pbl_profiler_start();

  for (unsigned int round = 0; round < ROUNDS; round++) {
    PBL_PROFILER_NODE_START(shuffle);
    prv_shuffle(round);
    PBL_PROFILER_NODE_STOP(shuffle);

    PBL_PROFILER_NODE_START(bubble_sort);
    prv_bubble_sort();
    PBL_PROFILER_NODE_STOP(bubble_sort);

    PBL_PROFILER_NODE_START(shuffle);
    prv_shuffle(round);
    PBL_PROFILER_NODE_STOP(shuffle);

    PBL_PROFILER_NODE_START(insertion_sort);
    prv_insertion_sort();
    PBL_PROFILER_NODE_STOP(insertion_sort);
  }

  /* Stops the run and logs a line per node, the busiest first. */
  pbl_profiler_print_stats();

  printf("profiler: %u sorts of %u values each way\n",
         (unsigned int)PBL_PROFILER_NODE_COUNT(bubble_sort), VALUES);
  printf("profiler: bubble sort %u us, insertion sort %u us\n",
         (unsigned int)PBL_PROFILER_NODE_TOTAL_US(bubble_sort),
         (unsigned int)PBL_PROFILER_NODE_TOTAL_US(insertion_sort));

  uint32_t bubble = PBL_PROFILER_NODE_TOTAL_CYCLES(bubble_sort);
  uint32_t insertion = PBL_PROFILER_NODE_TOTAL_CYCLES(insertion_sort);

  printf("profiler: insertion sort took fewer cycles: %s\n", insertion < bubble ? "yes" : "no");

  printf("Sample done\n");

  return 0;
}
