/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>

void serial_console_init(void);

bool serial_console_is_logging_enabled(void);
bool serial_console_is_prompt_enabled(void);

//! Allow the prompt to be started. By default the prompt is disabled it at system boot, and needs
//! to be enabled once the rest of the system is ready to handle prompt commands.
//! FIXME: This is probably in the wrong place, but we're reworking prompt in general once PULSEv2
//! lands so no need to rearrange the deck chairs on the titanic.
void serial_console_enable_prompt(void);

void serial_console_write_log_message(const char* msg);
