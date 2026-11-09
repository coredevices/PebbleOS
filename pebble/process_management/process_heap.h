/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

struct pbl_heap;
typedef struct PebbleProcessMd PebbleProcessMd;

//! Configure exception handlers (corruption and double-free) for
//! app and worker heaps.
void process_heap_set_exception_handlers(struct pbl_heap *heap, const PebbleProcessMd *app_md);
