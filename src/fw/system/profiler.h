/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

/* Setting up a profiler node:
 *  1. Create a new profiler node by adding it to profiler_list.h.
 *  2. Place PROFILER_NODE_START(<node>) and PROFILER_NODE_STOP(<node>) as desired.
 *  3. Make sure you are building with the "--profiler" configure option.
 *
 * Starting the profiler:
 *  The prompt commands "profiler start" and "profiler stop" can be used to toggle it from the
 *   command line.
 *  Alternatively, one can use the PROFILER_START and PROFILER_STOP macros to start and stop them at
 *   a specific point.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "util/list.h"

#if PROFILER
#define CMSIS_COMPATIBLE
#include <mcu.h>
#endif

typedef struct {
  ListNode list_node;
  char *module_name;
  uint32_t start;
  uint32_t end;
  uint32_t total;
  uint32_t count;
} ProfilerNode;

typedef struct {
  uint32_t start;
  uint32_t end;
  ListNode *nodes;
} Profiler;

extern Profiler g_profiler;

#if !defined(PROFILER)

#define PROFILER_NODE(name)
#define PROFILER_INIT
#define PROFILER_START
#define PROFILER_STOP
#define PROFILER_NODE_START(node)
#define PROFILER_NODE_STOP(node)
#define SYS_PROFILER_NODE_START(node)
#define SYS_PROFILER_NODE_STOP(node)
#define PROFILER_PRINT_STATS
#define PROFILER_NODE_GET_TOTAL_US(node) (0)
#define PROFILER_NODE_GET_TOTAL_CYCLES(node) (0)
#define PROFILER_NODE_GET_COUNT(node) (0)
#define PROFILER_NODE_GET_LAST_CYCLES(node) (0)

#else

#define PROFILER_NODE(name) extern ProfilerNode g_profiler_node_##name;
#include "profiler_list.h"
#undef PROFILER_NODE

#define PROFILER_INIT profiler_init()

#define PROFILER_START profiler_start()

#define PROFILER_STOP profiler_stop()

#define PROFILER_NODE_START(node) \
  g_profiler_node_##node.start = DWT->CYCCNT

#define PROFILER_NODE_STOP(node) \
  profiler_node_stop(&g_profiler_node_##node, DWT->CYCCNT)

#define SYS_PROFILER_NODE_START(node) \
  sys_profiler_node_start(&g_profiler_node_##node)

#define SYS_PROFILER_NODE_STOP(node) \
  sys_profiler_node_stop(&g_profiler_node_##node)

#define PROFILER_PRINT_STATS \
  profiler_print_stats()

#define PROFILER_NODE_GET_TOTAL_US(node) \
  profiler_node_get_total_us(&g_profiler_node_##node)

#define PROFILER_NODE_GET_LAST_CYCLES(node) \
  profiler_node_get_last_cycles(&g_profiler_node_##node)

#define PROFILER_NODE_GET_TOTAL_CYCLES(node) \
  g_profiler_node_##node.total

#define PROFILER_NODE_GET_COUNT(node) \
  profiler_node_get_count(&g_profiler_node_##node)

#endif // PROFILER

void profiler_init(void);
void profiler_print_stats(void);
void profiler_start(void);
void profiler_stop(void);
uint32_t profiler_cycles_to_us(uint32_t cycles);
void profiler_node_stop(ProfilerNode *node, uint32_t dwt_cyc_cnt);
uint32_t profiler_node_get_last_cycles(ProfilerNode *node);
uint32_t profiler_node_get_total_us(ProfilerNode *node);
uint32_t profiler_node_get_count(ProfilerNode *node);
//! returns total time elapsed between a start and stop call
//! @param in_us if true result is in us, else it's in cycles
uint32_t profiler_get_total_duration(bool in_us);

void sys_profiler_init(void);
void sys_profiler_node_start(ProfilerNode *node);
void sys_profiler_node_stop(ProfilerNode *node);
void sys_profiler_start(void);
void sys_profiler_stop(void);
void sys_profiler_print_stats(void);
