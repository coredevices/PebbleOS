/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */
#include "applib/app.h"
#include "applib/app_logging.h"
#include "applib/moddable/moddable.h"
#include "syscall/syscall_internal.h"

DEFINE_SYSCALL(void, moddable_createMachine, ModdableCreationRecord *cr)
{
	PBL_LOG_ERR("Moddable XS not supported in this build");
}