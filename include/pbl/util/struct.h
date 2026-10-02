/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

/**
 * @defgroup util_struct Structure access
 * @ingroup util
 * @brief Structure access helpers.
 * @{
 */

/**
 * @brief Read a structure field through a pointer that may be NULL.
 *
 * @param struct_ptr Pointer to the structure, evaluated twice.
 * @param field_name Field to read.
 * @param default_value Value when @p struct_ptr is NULL.
 * @return <tt>struct_ptr->field_name</tt>, or @p default_value.
 */
#define NULL_SAFE_FIELD_ACCESS(struct_ptr, field_name, default_value) \
  ((struct_ptr) ? ((struct_ptr)->field_name) : (default_value))

/** @} */
