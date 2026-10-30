/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */
#pragma once

#include <stdbool.h>

struct sxAPI;
struct sxMachine;

//! Whether @p the is the running machine, with the XS stack it was created with.
bool moddable_machine_is_current(const struct sxMachine *the);

//! Run app code unprivileged from the privileged XS machine. App RAM, where app code lives, is
//! executable only for the duration of the call.
void moddable_call_unprivileged(void (*fn)(void *), void *ctx);

//! Run the app's FFI builder, if it supplied one.
void moddable_build_ffi(struct sxMachine *the, struct sxAPI *api);
