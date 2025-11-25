/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

typedef struct Heap Heap;
typedef struct PebbleProcessMd PebbleProcessMd;

//! Configure exception handlers (corruption and double-free) for
//! app and worker heaps.
void process_heap_set_exception_handlers(Heap *heap,
                                         const PebbleProcessMd *app_md);
