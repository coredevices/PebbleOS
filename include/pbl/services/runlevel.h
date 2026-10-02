/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

/**
 * @defgroup services_runlevel Runlevels
 * @ingroup services
 * @brief Global switch of the system operating mode.
 *
 * Roughly analogous to SysVinit runlevels. In each runlevel, a service controlled by the runlevel
 * system is either enabled (operating normally) or disabled (suppressed, non-operational). The
 * exact meaning of the enabled state is up to the service.
 *
 * The enabled state acts like a circuit breaker for a light switch: with the breaker off, the
 * light is off whatever the switch position; with it on, the switch decides. Many services let
 * the switch be flipped while disabled, the change taking effect once enabled.
 *
 * A controllable service provides a single @c service_set_enabled(bool) function that:
 * - is only called by the runlevel system,
 * - is idempotent,
 * - and expects the service to start disabled when initialized.
 *
 * Starting disabled avoids an uninitialized, enabled, disabled sequence when booting into a
 * runlevel where the service is off, and exercises the disabled to enabled path on every boot.
 *
 * There is deliberately no @c services_get_runlevel(): code must not change behaviour based on
 * the runlevel without being listed and controlled by services_set_runlevel(). Make the service
 * controllable by the runlevel system instead.
 *
 * Runlevels are defined in runlevel.def; see @ref services_runlevel_impl for enable masks.
 * @{
 */

/**
 * @brief System runlevel.
 *
 * One @c RunLevel_<name> value per entry of runlevel.def: @c RunLevel_BareMinimum,
 * @c RunLevel_LowPower, @c RunLevel_Stationary, @c RunLevel_FirmwareUpdate and
 * @c RunLevel_Normal.
 */
typedef enum RunLevel {
#define RUNLEVEL(number, name) RunLevel_##name = number,
#include "runlevel.def"
#undef RUNLEVEL
  /** Number of runlevels. */
  RunLevel_COUNT
} RunLevel;

/**
 * @brief Switch the system to a runlevel.
 *
 * Enables or disables every runlevel-controlled service according to its enable mask.
 *
 * @param runlevel Runlevel to switch to.
 */
void services_set_runlevel(RunLevel runlevel);

/** @} */
