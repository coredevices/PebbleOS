/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

//! Returns `struct_ptr->field_name` if `struct_ptr` isn't NULL, otherwise returns default_value
#define NULL_SAFE_FIELD_ACCESS(struct_ptr, field_name, default_value) \
  ((struct_ptr) ? ((struct_ptr)->field_name) : (default_value))
