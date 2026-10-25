/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

// With KernelBG, below the app and NimBLE's host and HCI threads: LE traffic and
// the UI keep priority, and a busy host cannot starve KernelBG into a watchdog
// reset.
#define CONFIG_ZBT_EXECUTOR_PRIO (PBL_PRIO_IDLE + 1)

struct zbt_stack;
struct zbt_host_threads;
typedef struct MpuRegion MpuRegion;

//! The thread runs on @p threads (its exec members); @p regions: MPU regions for the thread
//! (PBL_THREAD_MAX_MEM_REGIONS entries), or NULL.
void zbt_executor_start(struct zbt_host_threads *threads, const MpuRegion *const *regions);
void zbt_executor_quit(void);
bool zbt_executor_is_current(void);

typedef void (*zbt_rx_handler_t)(void *buf);
void zbt_rx_thread_start(zbt_rx_handler_t handler, struct zbt_host_threads *threads,
                         const MpuRegion *const *regions);
void zbt_rx_thread_put(void *buf);
void zbt_rx_thread_quit(void);
bool zbt_rx_thread_is_current(void);

bool zbt_threads_stopped(void);
//! Work queue loop passes and HCI packets handed to the host, to tell a busy loop.
void zbt_threads_counts(uint32_t *exec_loops, uint32_t *rx_items);
void zbt_threads_stacks(struct zbt_stack *exec, struct zbt_stack *rx);
