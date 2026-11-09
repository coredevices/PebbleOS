/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <kernel/pebble_tasks.h>

typedef struct PebbleProcessMd PebbleProcessMd;

//! How the code of a loaded process runs. By default the CPU runs it, at the
//! addresses it was loaded to; a runtime that interprets processes, as native
//! builds do with Samsa, provides its own.

//! The address a process sees for an address in its image.
uint32_t process_runtime_address(PebbleTask task, const void *address);

//! The address of the SDK's function table a process sees.
uint32_t process_runtime_sdk_table(PebbleTask task);

//! Run a process from its entry point, on the process's own task, until its
//! main function returns.
void process_runtime_run(const PebbleProcessMd *app_md, PebbleTask task, void *entry,
                         void *stack_top);
