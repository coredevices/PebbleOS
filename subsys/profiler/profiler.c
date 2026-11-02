/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "profiler_priv.h"

#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include <pbl/kernel/section.h>
#include <pbl/kernel/syscall.h>
#include <pbl/kernel/time.h>
#include <pbl/logging/logging.h>
#include <pbl/profiler/profiler.h>

PBL_LOG_MODULE_DEFINE(profiler, CONFIG_PROFILER_LOG_LEVEL);

#ifndef PBL_NO_LINKER_SCRIPT
extern struct pbl_profiler_node *const __pbl_profiler_nodes_start[];
extern struct pbl_profiler_node *const __pbl_profiler_nodes_end[];
#else
extern struct pbl_profiler_node *const __pbl_profiler_nodes_start[] PBL_UNSORTED_SECTION_START(
    pbl_prof_nodes);
extern struct pbl_profiler_node *const __pbl_profiler_nodes_end[] PBL_UNSORTED_SECTION_END(
    pbl_prof_nodes);
#endif

#define FOREACH_NODE(_ref)                                                 \
  for (struct pbl_profiler_node *const *_ref = __pbl_profiler_nodes_start; \
       _ref < __pbl_profiler_nodes_end; _ref++)

static uint32_t s_start;
static uint32_t s_end;

static bool prv_is_node(const struct pbl_profiler_node *node) {
  FOREACH_NODE(ref) {
    if (*ref == node) {
      return true;
    }
  }
  return false;
}

void pbl_profiler_reset(void) {
  s_start = 0;
  s_end = 0;
  FOREACH_NODE(ref) {
    (*ref)->start = 0;
    (*ref)->total = 0;
    (*ref)->count = 0;
  }
}

void pbl_profiler_start(void) {
  s_start = pbl_cycle_get_32();
}

void pbl_profiler_stop(void) {
  s_end = pbl_cycle_get_32();
}

uint32_t pbl_profiler_duration_cycles(void) {
  return s_end - s_start;
}

uint32_t pbl_profiler_cycles_to_us(uint32_t cycles) {
  return (uint32_t)(((uint64_t)cycles * 1000000U) / pbl_cycles_per_sec());
}

PBL_SYSCALL(void, pbl_profiler_node_start, (VAL, struct pbl_profiler_node *, node)) {
  if (from_user && !prv_is_node(node)) {
    return;
  }

  node->start = pbl_cycle_get_32();
}

PBL_SYSCALL(void, pbl_profiler_node_stop, (VAL, struct pbl_profiler_node *, node)) {
  uint32_t now = pbl_cycle_get_32();

  if (from_user && !prv_is_node(node)) {
    return;
  }

  node->count++;
  node->total += now - node->start;
}

// Whether a comes before b in the stats: most cycles first, then link order.
static bool prv_before(struct pbl_profiler_node *const *a, struct pbl_profiler_node *const *b) {
  return ((*a)->total > (*b)->total) || (((*a)->total == (*b)->total) && (a < b));
}

void pbl_profiler__stats(pbl_profiler__emit_t emit, void *ctx) {
  pbl_profiler_stop();

  uint32_t total = pbl_profiler_duration_cycles();
  char buf[96];

  snprintf(buf, sizeof(buf), "Ran for %" PRIu32 " cycles (%" PRIu32 " us) at %" PRIu32 " Hz", total,
           pbl_profiler_cycles_to_us(total), pbl_cycles_per_sec());
  emit(buf, ctx);

  snprintf(buf, sizeof(buf), "%-24s %-8s %-11s %-15s %-8s %-7s", "Name", "Count", "Cycles",
           "Time (us)", "Avg (us)", "% CPU");
  emit(buf, ctx);

  struct pbl_profiler_node *const *prev = nullptr;
  while (true) {
    struct pbl_profiler_node *const *next = nullptr;
    FOREACH_NODE(ref) {
      if (((prev == nullptr) || prv_before(prev, ref)) &&
          ((next == nullptr) || prv_before(ref, next))) {
        next = ref;
      }
    }
    if (next == nullptr) {
      break;
    }
    prev = next;

    const struct pbl_profiler_node *node = *next;
    if (node->count == 0) {
      continue;
    }

    uint32_t percent = (total != 0) ? (uint32_t)(((uint64_t)node->total * 100U) / total) : 0;
    snprintf(buf, sizeof(buf),
             "%-24.24s %-8" PRIu32 " %-11" PRIu32 " %-15" PRIu32 " %-8" PRIu32 " %-7" PRIu32,
             node->name, node->count, node->total, pbl_profiler_cycles_to_us(node->total),
             pbl_profiler_cycles_to_us(node->total / node->count), percent);
    emit(buf, ctx);
  }
}

static void prv_emit_log(const char *line, void *ctx) {
  PBL_LOG_INFO("%s", line);
}

void pbl_profiler_print_stats(void) {
  pbl_profiler__stats(prv_emit_log, nullptr);
}
