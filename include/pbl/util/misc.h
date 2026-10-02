/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>
#include <stdint.h>

/**
 * @defgroup util Utilities
 * @ingroup lib
 * @brief Generic helpers shared by the firmware, the bootloader and host tools (@c lib/util).
 *
 * The library does not depend on the kernel. Logging and assertions go through util_log() and
 * util_assertion_failed(), weak functions the firmware overrides.
 *
 * @code{.c}
 * static const uint16_t s_rates[] = {10, 25, 50, 100};
 *
 * for (size_t i = 0; i < ARRAY_LENGTH(s_rates); i++) {
 *   max_rate = MAX(max_rate, s_rates[i]);
 * }
 * level = CLIP(level, 0, 100);
 *
 * struct item *item = container_of(node, struct item, list_node);
 * @endcode
 */

/**
 * @defgroup util_misc Miscellaneous
 * @ingroup util
 * @brief Small generic macros.
 * @{
 */

/**
 * @brief Get the structure that contains a member.
 *
 * @param ptr Pointer to the member.
 * @param type Type of the containing structure.
 * @param member Name of the member within @p type.
 * @return Pointer to the containing structure.
 */
#define container_of(ptr, type, member) ((type *)((char *)(ptr) - (size_t)&(((type *)0)->member)))

/**
 * @brief Swap the values of two lvalues of the same type.
 *
 * @param a First lvalue, evaluated more than once.
 * @param b Second lvalue, evaluated more than once.
 */
#define PBL_SWAP(a, b)                 \
  do {                                 \
    __typeof__(a) _pbl_swap_tmp = (a); \
    (a) = (b);                         \
    (b) = _pbl_swap_tmp;               \
  } while (0)

/**
 * @brief Pack four characters into a @c uint32_t, the first one in the most significant byte.
 *
 * @param a First character.
 * @param b Second character.
 * @param c Third character.
 * @param d Fourth character.
 */
#define PBL_FOURCC(a, b, c, d)                                                  \
  ((uint32_t)(((uint32_t)(uint8_t)(a) << 24) | ((uint32_t)(uint8_t)(b) << 16) | \
              ((uint32_t)(uint8_t)(c) << 8) | (uint32_t)(uint8_t)(d)))

/** @} */
