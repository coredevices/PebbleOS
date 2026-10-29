/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <pbl/services/mic_manager.h>

//! Records from the board microphone and hands frames of @p buffer_len samples to @p handler on
//! KernelBG. The buffer is owned by the source until mic_source_stop() returns.
bool mic_source_start(MicDataHandlerCB handler, void *context, int16_t *buffer, size_t buffer_len);

//! Stops recording. No handler runs once it returns.
void mic_source_stop(void);
