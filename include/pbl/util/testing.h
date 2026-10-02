/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "pbl/kernel/compiler.h"

/**
 * @defgroup util_testing Unit test hooks
 * @ingroup util
 * @brief Linkage helpers that let unit tests override firmware functions.
 * @{
 */

#if UNITTEST
#define PBL_T_STATIC PBL_WEAK
#else
/** @brief @c static in firmware, weak in unit tests so private functions can be overridden. */
#define PBL_T_STATIC static
#endif

#if UNITTEST
#define PBL_T_MOCKABLE PBL_WEAK
#else
/** @brief Nothing in firmware, weak in unit tests so global functions can be mocked. */
#define PBL_T_MOCKABLE
#endif

/** @} */
