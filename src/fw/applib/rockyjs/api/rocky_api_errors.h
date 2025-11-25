/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "jerry-api.h"

// Raise TypeError: Not enough arguments
jerry_value_t rocky_error_arguments_missing(void);

jerry_value_t rocky_error_argument_invalid_at_index(uint32_t arg_idx, const char *error_msg);

jerry_value_t rocky_error_argument_invalid(const char *msg);

jerry_value_t rocky_error_oom(const char *hint);

jerry_value_t rocky_error_unexpected_type(uint32_t arg_idx, const char *expected_type_name);

// Print error type & msg
void rocky_error_print(jerry_value_t error_val);
