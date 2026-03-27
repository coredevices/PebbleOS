/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once
#ifndef RECOVERY_FW
void init_memfault_chunk_collection(void);
void memfault_chunk_collect(void);
void memfault_chunk_collect_after_delay(void);
#else
static inline void init_memfault_chunk_collection(void) {}
static inline void memfault_chunk_collect(void) {}
static inline void memfault_chunk_collect_after_delay(void) {}
#endif