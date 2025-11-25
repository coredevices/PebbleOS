/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once
#include <stdbool.h>

//! Enters nudging mode with the layer at given address
void command_layer_nudge(const char *address_str);

//! Prompt character handler
void layer_debug_nudging_handle_character(char c, bool *should_context_switch);
