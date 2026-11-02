/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

typedef void (*pbl_profiler__emit_t)(const char *line, void *ctx);

//! Stop the run and pass each line of the stats to @p emit.
void pbl_profiler__stats(pbl_profiler__emit_t emit, void *ctx);
