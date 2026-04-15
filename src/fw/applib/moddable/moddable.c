/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */
#include "applib/app.h"
#include "kernel/logging_private.h"
#include "pbl/services/common/evented_timer.h"
#include "syscall/syscall_internal.h"
#include "applib/app_logging.h"
#include "applib/moddable/moddable.h"

#include <stddef.h>

#if CAPABILITY_HAS_MODDABLE_XS && !defined(RECOVERY_FW)
#include "xsmc.h"
#include "xsHost.h"
#include "xsHosts.h"
#include "moddableAppState.h"
#include "kernel/pbl_malloc.h"

static void startMachine(void *data)
{
	modRunMachineSetup((xsMachine *)data);	// want this to be called after event loop is active
}

void moddable_cleanup(void)
{
	ModdablePebbleAppState state = (ModdablePebbleAppState)app_state_get_js_memory_api_context();

	xsDeleteMachine(state->the);

	extern void modTimerExit(void);
	modTimerExit();

	app_state_set_js_memory_api_context(NULL);
	task_free(state);
}

// Minimum recordSize for the original struct (without flags field)
#define kModdableCreationRecordMinSize offsetof(ModdableCreationRecord, flags)

DEFINE_SYSCALL(void, moddable_createMachine, ModdableCreationRecord *cr)
{
	xsMachine *the;
	uint32_t flags = 0;

	if (NULL == cr)
		the = modCloneMachine(NULL, NULL);
	else {
		if (cr->recordSize < kModdableCreationRecordMinSize) {
			PBL_LOG_ERR("invalid recordSize");
			return;
		}

		// Read flags if the record is large enough to include them
		if (cr->recordSize >= sizeof(ModdableCreationRecord))
			flags = cr->flags;

		uint32_t stack = cr->stack, slot = cr->slot, chunk = cr->chunk;
		if (!stack && !slot && !chunk)
			the = modCloneMachine(NULL, NULL);
		else  {
			if (!stack || !slot || !chunk) {
				PBL_LOG_ERR("invalid ModdableCreationRecord");
				return;
			}

			stack = (stack + 3) & ~3;
			slot = (slot + 3) & ~3;
			chunk = (chunk + 3) & ~3;

			xsCreation *defaultCreation;
			extern void *xsPreparationAndCreation(xsCreation **creation);
			(void)xsPreparationAndCreation(&defaultCreation);
			struct xsCreationRecord creation = *defaultCreation;
			creation.stackCount = stack / sizeof(xsSlot);
			creation.initialHeapCount = slot / sizeof(xsSlot);
			creation.initialChunkSize = chunk;
			if ((stack + slot + chunk) <= (uint32_t)creation.staticSize)
				creation.staticSize = stack + slot + chunk;
			else {
				creation.incrementalChunkSize = 0;
				creation.incrementalHeapCount = 0;
				creation.staticSize = 0;
			}
			the = modCloneMachine(&creation, NULL);
		}
	}

	if (NULL == the) {
		PBL_LOG_ERR("Failed to create XS machine");
		return;
	}

	// Don't log instrumentation if nobody is listening to APP_LOG over BT
	if (!app_log_is_bt_enabled())
		flags &= ~kModdableCreationFlagLogInstrumentation;

	ModdablePebbleAppState state = task_zalloc_check(sizeof(ModdablePebbleAppStateRecord));
	state->the = the;
	state->eventedTimer = EVENTED_TIMER_INVALID_ID;
	state->creationFlags = flags;
	app_state_set_js_memory_api_context((void *)state);

	evented_timer_register(2, false, startMachine, the);

	app_event_loop();

	moddable_cleanup();
}

#else

DEFINE_SYSCALL(void, moddable_createMachine, ModdableCreationRecord *cr)
{
	PBL_LOG_ERR("Moddable XS not supported in this build");
}
#endif
