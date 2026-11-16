/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdarg.h>
#include <stddef.h>

#include <pbl/kernel/compiler.h>

/**
 * @defgroup kernel_print Print
 * @ingroup kernel
 * @brief Plain text for the system's own messages, such as the boot banner.
 *
 * Text goes to the output registered with pbl_print_set_output(), usually the console, before
 * pbl_print() returns. It is never filtered or recorded: logging is for that. It works from any
 * context, before the scheduler runs too, and is dropped while there is no output.
 *
 * @code{.c}
 * pbl_print("*** Booting PebbleOS %s ***\n", PBL_VERSION_TAG);
 * @endcode
 * @{
 */

/**
 * @brief Output of pbl_print().
 *
 * @param data Text, without a terminating NUL.
 * @param len Length of @p data in bytes.
 */
typedef void (*pbl_print_output_t)(const char *data, size_t len);

/**
 * @brief Set where pbl_print() writes.
 *
 * @param output Output, or NULL to drop the text.
 */
void pbl_print_set_output(pbl_print_output_t output);

/**
 * @brief Print formatted text. Lines end with a newline the caller writes.
 *
 * Text is cut at CONFIG_KERNEL_PRINT_BUFFER_SIZE - 1 bytes per call.
 *
 * @param fmt printf-style format.
 * @param ... Format arguments.
 */
void pbl_print(const char *fmt, ...) PBL_FORMAT_PRINTF(1, 2);

/**
 * @brief pbl_print() with a va_list.
 *
 * @param fmt printf-style format.
 * @param ap Format arguments.
 */
void pbl_vprint(const char *fmt, va_list ap) PBL_FORMAT_PRINTF(1, 0);

/** @} */
