/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

/**
 * @defgroup services_runlevel_impl Runlevel enable masks
 * @ingroup services_runlevel
 * @brief Definitions used to implement runlevels.
 *
 * Runlevels are defined in the runlevel.def X-macro file, from which RunLevel and RunLevelBit
 * are built. A service's enable mask is the bitwise OR of the RunLevelBit of every runlevel it
 * is enabled in; it is enabled in @c runlevel when @c (enable_mask & (1 << runlevel)) != 0.
 *
 * The short @c R_<name> names keep masks readable. Only include this header from the files that
 * define masks.
 *
 * @code{.c}
 * static const struct ServiceRunLevelSetting s_setting = {
 *   .set_enable_fn = light_allow,
 *   .enable_mask = R_LowPower | R_FirmwareUpdate | R_Normal,
 * };
 * @endcode
 * @{
 */

/** @cond INTERNAL_HIDDEN */
#define RUNLEVEL(number, name)                                                   \
  static_assert(0 <= number && number <= 31,                                     \
                "The numeric value of runlevel " #name " (" #number              \
                ")"                                                              \
                " is out of range. Only runlevels in the range 0 <= level <= 31" \
                " are supported.");
#include "runlevel.def"
#undef RUNLEVEL
/** @endcond */

/**
 * @brief Bit of each runlevel in an enable mask.
 *
 * One @c R_<name> value, @c (1 << number), per entry of runlevel.def.
 */
typedef enum RunLevelBit {
#define RUNLEVEL(number, name) R_##name = (1 << number),
#include "runlevel.def"
#undef RUNLEVEL
} RunLevelBit;

/** @brief Runlevel control of one service. */
struct ServiceRunLevelSetting {
  /** Enable or disable the service. */
  void (*set_enable_fn)(bool);
  /** RunLevelBit values of the runlevels the service is enabled in. */
  RunLevelBit enable_mask;
};

/** @} */
