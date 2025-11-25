/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

//! \file
//! The runlevel system provides a global switch for changing the system's
//! operating mode, roughly analogous to runlevels in UNIX SysVinit. For each
//! runlevel, a service can be in either an enabled or disabled state. When a
//! service is disabled, it is "off" or "suppressed"; it is non-operational.
//! When a service is enabled, it operates normally.
//!
//! The exact semantics of the enabled state is left up to the individual
//! service. For some services, it may even make sense that in certain
//! situations the enabled state is functionally indistinguishable from the
//! disabled state. A service's enabled and disabled states are analogous to a
//! circuit breaker for a light switch: when the circuit breaker is off, the
//! light is off regardless of the position of the light switch. When the
//! circuit breaker is turned on, the light switch determines whether the light
//! is on or off. Following this analogy further; for many services it makes
//! sense to allow the light switch to be flipped while the circuit breaker is
//! off. While this state change is not reflected immediately, it takes effect
//! as soon as the circuit breaker is enabled.
//!
//! Services need to be modified to be controllable through the runlevel system.
//! The service needs to have a single `service_set_enabled(bool)` function
//! which enables or disables the service as described above. This function
//! should never be called outside of the runlevel system. This function must be
//! idempotent: calling it any number of times with the same argument must
//! have the same effect as calling it once. And the service must start in the
//! disabled state when first initialized.
//!
//! The final requirement, that services must start up disabled, is in place for
//! two reasons. The system may boot into a runlevel for which the service is
//! disabled, and rapidly switching from uninitialized -> enabled -> disabled is
//! undesirable. And requiring that each service be explicitly enabled as a
//! separate step from initialization ensures that the disabled -> enabled state
//! change code path is exercised on every normal boot, making it more likely
//! that bugs in the service's state change code are noticed sooner.
//!
//! More details on the design of the runlevel system can be found in the design
//! doc at
//! https://docs.google.com/a/pulse-dev.net/document/d/1jz1xRvItR228ufFS8Fcf70lhwoZ7DI7G1MBD2hymVAs/edit
//!
//! \par Why is there no `services_get_runlevel()` function?
//! Having such a function would make it possible for code to alter its
//! behaviour based on runlevel without properly integrating into the runlevel
//! system. This would subvert the property that all services controlled by the
//! runlevel system are centrally listed and controlled by the
//! `services_set_runlevel()` function. Anything that could be accomplished with
//! `services_get_runlevel()` can be better accomplished by making that service
//! controllable by the runlevel system in the usual way.

// Runlevels are defined in runlevel.def. DO NOT ADD NEW RUNLEVELS TO THIS FILE!

typedef enum RunLevel {
#define RUNLEVEL(number, name) RunLevel_##name = number,
#include "runlevel.def"
#undef RUNLEVEL
  RunLevel_COUNT
} RunLevel;

void services_set_runlevel(RunLevel runlevel);

