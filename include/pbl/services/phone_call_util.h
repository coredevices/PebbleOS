/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

/**
 * @addtogroup services_phone_call
 * @{
 */

/** @brief Caller identity carried by phone events. */
typedef struct PebblePhoneCaller {
  /** Phone number, kernel heap allocated, or NULL. */
  char *number;
  /** Caller name, kernel heap allocated, or NULL. */
  char *name;
} PebblePhoneCaller;

/**
 * @brief Create a caller to pass in a phone event.
 *
 * The strings are copied to the kernel heap. When both are NULL or empty, the name is set to the
 * localized "Unknown".
 *
 * @param number Phone number, or NULL.
 * @param name Caller name, or NULL.
 * @return New caller, or NULL if out of memory.
 */
PebblePhoneCaller *phone_call_util_create_caller(const char *number, const char *name);

/**
 * @brief Free a caller created with phone_call_util_create_caller().
 *
 * @param caller Caller to free, or NULL.
 */
void phone_call_util_destroy_caller(PebblePhoneCaller *caller);

/** @} */
