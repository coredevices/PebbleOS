/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-FileCopyrightText: 2025-2026 Moddable Tech, Inc. */
/* SPDX-License-Identifier: Apache-2.0 */
#include "applib/app.h"
#include "kernel/logging_private.h"
#include "pbl/services/evented_timer.h"
#include "syscall/syscall_internal.h"
#include "applib/app_logging.h"
#include "applib/moddable/moddable.h"
#include "applib/ui/dialogs/expandable_dialog.h"
#include "applib/ui/dialogs/simple_dialog.h"

#include <stddef.h>

#if defined(CONFIG_MODDABLE_XS) && !defined(CONFIG_RECOVERY_FW)
#include "xsmc.h"
#include "xsHost.h"
#include "xsHosts.h"
#include "moddableAppState.h"
#include "kernel/pbl_malloc.h"

void moddable_cleanup(void)
{
	ModdablePebbleAppState state = (ModdablePebbleAppState)app_state_get_js_memory_api_context();

	if (state->the)
		xsDeleteMachine(state->the);

	if (state->abortReason)
		c_free(state->abortReason);

	extern void modTimerExit(void);
	modTimerExit();

	while (state->debugFragments) {
		DebugFragment f = state->debugFragments;
		state->debugFragments = f->next;
		kernel_free(f);
	}

	app_state_set_js_memory_api_context(NULL);
	task_free(state);
}

// Minimum recordSize for the original struct (without flags field)
#define kModdableCreationRecordMinSize offsetof(ModdableCreationRecord, flags)

DEFINE_SYSCALL(void, moddable_createMachine, ModdableCreationRecord *cr)
{
	uint32_t flags = 0;

	ModdablePebbleAppState state = task_zalloc_check(sizeof(ModdablePebbleAppStateRecord));
	app_state_set_js_memory_api_context((void *)state);

	// Read flags if the record is large enough to include them
	if (cr && (cr->recordSize >= (offsetof(ModdableCreationRecord, flags) + sizeof(uint32_t))))
		flags = cr->flags;

	// Don't log instrumentation if nobody is listening to APP_LOG over BT
	if (!app_log_is_bt_enabled())
		flags &= ~(kModdableCreationFlagLogInstrumentation | kModdableCreationFlagDebug);

	state->creationFlags = flags;

	void *fxBuildFFI = NULL;
	xsCreation *defaultCreation;
	extern void *xsPreparationAndCreation(xsCreation **creation);
	(void)xsPreparationAndCreation(&defaultCreation);
	struct xsCreationRecord creation = *defaultCreation;
	if (NULL != cr) {
		if (cr->recordSize < kModdableCreationRecordMinSize) {
			APP_LOG(APP_LOG_LEVEL_ERROR, "invalid recordSize");
			return;
		}

		uint32_t stack = (cr->stack + 3) & ~3, slot = (cr->slot + 3) & ~3, chunk = (cr->chunk + 3) & ~3;
		if (stack || slot || chunk) {
			if (!stack || !slot || !chunk) {
				APP_LOG(APP_LOG_LEVEL_ERROR, "invalid ModdableCreationRecord");
				return;
			}

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
		}

		if ((offsetof(ModdableCreationRecord, fxBuildFFI) + sizeof(fxBuildFFI)) <= cr->recordSize) {
			fxBuildFFI = cr->fxBuildFFI;

			if (fxBuildFFI && creation.staticSize) {
				int available = creation.staticSize - (creation.stackCount * sizeof(xsSlot));
				creation.initialHeapCount = (available >> 1) / sizeof(xsSlot);
				creation.initialChunkSize = available >> 1;		
			}
		}
	}

	// FFI hands the app direct pointers into XS storage (string chunks, slot
	// handles); keep the whole machine in app RAM so unprivileged dereferences
	// don't MPU-fault.
	modMachineAllowKernelHeap(NULL == fxBuildFFI);

	xsMachine *the = modCloneMachine(&creation, NULL);
	if (NULL == the) {
		APP_LOG(APP_LOG_LEVEL_ERROR, "failed to allocate XS machine");
		moddable_cleanup();
		return;
	}

	state->the = the;
	state->fxBuildFFI = fxBuildFFI;
	state->eventedTimer = EVENTED_TIMER_INVALID_ID;

	evented_timer_register(1, false, (EventedTimerCallback)modRunMachineSetup, the);

	xsBeginHostExit(the);
	app_event_loop();
	xsEndHostExit(the);

	int exitStatus = the->exitStatus;
	char *abortReason = state->abortReason;
	state->abortReason = NULL;
	moddable_cleanup();

	if ((xsNormalExit != exitStatus) && (xsDebuggerExit != exitStatus)) {
		ExpandableDialog *dialog = expandable_dialog_create("");
		Dialog *base_dialog = expandable_dialog_get_dialog(dialog);

		expandable_dialog_set_header(dialog, "Alloy: Fatal Error");
		char *msg = (char *)fxAbortString(exitStatus);
		dialog_set_text(base_dialog, abortReason ? abortReason : msg);
		if (abortReason) {
		    c_free(abortReason);
		}
		dialog_set_icon(base_dialog, RESOURCE_ID_GENERIC_WARNING_SMALL);
		dialog_set_fullscreen(base_dialog, true);
		expandable_dialog_show_action_bar(dialog, false);

		app_expandable_dialog_push(dialog);

		app_event_loop();
	}
}

#else

DEFINE_SYSCALL(void, moddable_createMachine, ModdableCreationRecord *cr)
{
	APP_LOG(APP_LOG_LEVEL_ERROR, "Moddable XS not supported in this build");
}

// Normally provided by the moddable submodule (xsPlatform.c). The xsbug
// endpoint stays in the protocol endpoints table regardless of build config,
// so stub the callback when building without moddable to satisfy the linker.
struct CommSession;

void xsbug_protocol_msg_callback(struct CommSession *session, const uint8_t *msg, size_t length)
{
}
#endif
