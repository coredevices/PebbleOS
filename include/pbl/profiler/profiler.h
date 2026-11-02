/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/kernel/compiler.h>
#include <pbl/kernel/section.h>

/**
 * @defgroup profiler Profiler
 * @ingroup subsys
 * @brief Cycle-counting profiler with named nodes.
 *
 * A node accumulates the cycles spent between its start and stop calls. Nodes are defined next
 * to the code they measure and collected at link time. A profiling run is reset with
 * pbl_profiler_reset(), timed with pbl_profiler_start() and pbl_profiler_stop(), and logged with
 * pbl_profiler_print_stats(), which shows each node's share of the run.
 *
 * The node macros compile to nothing without @c CONFIG_PROFILER.
 *
 * @code{.c}
 * PBL_PROFILER_NODE_DEFINE(render);
 *
 * void prv_render(void) {
 *   PBL_PROFILER_NODE_START(render);
 *   ...
 *   PBL_PROFILER_NODE_STOP(render);
 * }
 * @endcode
 * @{
 */

/** @brief A profiler node. */
struct pbl_profiler_node {
  /** Name, as shown in the stats. */
  const char *name;
  /** Cycle count at the last start. */
  uint32_t start;
  /** Cycles accumulated since the last reset. */
  uint32_t total;
  /** Stops since the last reset. */
  uint32_t count;
};

#ifdef CONFIG_PROFILER

#ifdef PBL_NO_LINKER_SCRIPT
#define PRV_PROFILER_NODE_SECTION(_name) PBL_UNSORTED_SECTION(pbl_prof_nodes)
#else
#define PRV_PROFILER_NODE_SECTION(_name) PBL_ALIGNED(4) PBL_SECTION(".pbl_profiler_nodes." #_name)
#endif

/**
 * @brief Define a profiler node.
 *
 * @param _name Name, an identifier unique across the firmware.
 */
#define PBL_PROFILER_NODE_DEFINE(_name)                                         \
  struct pbl_profiler_node pbl_profiler_node_##_name = {.name = #_name};        \
  static struct pbl_profiler_node *const pbl_profiler_node_ref_##_name PBL_USED \
  PRV_PROFILER_NODE_SECTION(_name) = &pbl_profiler_node_##_name

/**
 * @brief Declare a profiler node defined in another file.
 *
 * @param _name Name of the node.
 */
#define PBL_PROFILER_NODE_DECLARE(_name) extern struct pbl_profiler_node pbl_profiler_node_##_name

/** @brief Start timing a node. Callable from unprivileged code. */
#define PBL_PROFILER_NODE_START(_name) pbl_profiler_node_start(&pbl_profiler_node_##_name)

/** @brief Stop timing a node and add the elapsed cycles to it. Callable from unprivileged code. */
#define PBL_PROFILER_NODE_STOP(_name) pbl_profiler_node_stop(&pbl_profiler_node_##_name)

/** @brief Cycles a node accumulated since the last reset. */
#define PBL_PROFILER_NODE_TOTAL_CYCLES(_name) (pbl_profiler_node_##_name.total)

/** @brief Microseconds a node accumulated since the last reset. */
#define PBL_PROFILER_NODE_TOTAL_US(_name) pbl_profiler_cycles_to_us(pbl_profiler_node_##_name.total)

/** @brief Times a node was stopped since the last reset. */
#define PBL_PROFILER_NODE_COUNT(_name) (pbl_profiler_node_##_name.count)

/** @brief Clear the run and every node. */
void pbl_profiler_reset(void);

/** @brief Start the run. */
void pbl_profiler_start(void);

/** @brief Stop the run. */
void pbl_profiler_stop(void);

/**
 * @brief Get the cycles between the last start and stop of the run.
 *
 * @return Cycles elapsed.
 */
uint32_t pbl_profiler_duration_cycles(void);

/**
 * @brief Convert cycles to microseconds.
 *
 * @param cycles Cycles.
 * @return Microseconds at the current CPU clock.
 */
uint32_t pbl_profiler_cycles_to_us(uint32_t cycles);

/** @brief Stop the run and log the stats of every node. */
void pbl_profiler_print_stats(void);

/**
 * @brief Start timing a node.
 *
 * @param node Node; calls from unprivileged code with anything but a defined node are ignored.
 */
void pbl_profiler_node_start(struct pbl_profiler_node *node);

/**
 * @brief Stop timing a node and add the elapsed cycles to it.
 *
 * @param node Node; calls from unprivileged code with anything but a defined node are ignored.
 */
void pbl_profiler_node_stop(struct pbl_profiler_node *node);

#else

#define PBL_PROFILER_NODE_DEFINE(_name)       static_assert(true, "")
#define PBL_PROFILER_NODE_DECLARE(_name)      static_assert(true, "")
#define PBL_PROFILER_NODE_START(_name)        ((void)0)
#define PBL_PROFILER_NODE_STOP(_name)         ((void)0)
#define PBL_PROFILER_NODE_TOTAL_CYCLES(_name) (0U)
#define PBL_PROFILER_NODE_TOTAL_US(_name)     (0U)
#define PBL_PROFILER_NODE_COUNT(_name)        (0U)

#endif /* CONFIG_PROFILER */

/** @} */
