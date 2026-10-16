/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */
#pragma once

struct sxAPI;
struct sxMachine;

//! Run app code unprivileged from the privileged XS machine. App RAM, where app code lives, is
//! executable only for the duration of the call.
void moddable_call_unprivileged(void (*fn)(void *), void *ctx);

//! Run the app's FFI builder, if it supplied one.
void moddable_build_ffi(struct sxMachine *the, struct sxAPI *api);
