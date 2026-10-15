/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-FileCopyrightText: 2025-2026 Moddable Tech, Inc. */
/* SPDX-License-Identifier: Apache-2.0 */
#include <stddef.h>

#include <pbl/mcu/privilege.h>
#include <pbl/services/evented_timer.h>

#include <applib/app.h>
#include <applib/app_logging.h>
#include <applib/moddable/moddable.h>
#include <applib/ui/dialogs/expandable_dialog.h>
#include <logging/logging_private.h>
#include <syscall/syscall.h>
#include <syscall/syscall_define.h>

#if defined(CONFIG_MODDABLE_XS) && !defined(CONFIG_RECOVERY_FW)
#include <kernel/event_loop.h>
#include <kernel/pbl_malloc.h>
#include <kernel/ui/modals/modal_manager.h>
#include <moddableAppState.h>
#include <process_management/app_manager.h>
#include <shell/normal/watchface.h>
#include <xsHost.h>
#include <xsHosts.h>
#include <xsffi.h>
#include <xsmc.h>

static ExpandableDialog *prv_create_fatal_dialog(const char *message) {
  ExpandableDialog *dialog = expandable_dialog_create("");
  Dialog *base_dialog = expandable_dialog_get_dialog(dialog);
  expandable_dialog_set_header(dialog, "Alloy: Fatal Error");
  dialog_set_text(base_dialog, message);
  dialog_set_icon(base_dialog, RESOURCE_ID_GENERIC_WARNING_SMALL);
  dialog_set_fullscreen(base_dialog, true);
  expandable_dialog_show_action_bar(dialog, false);
  return dialog;
}

static void prv_watchface_fatal_error(void *data) {
  ExpandableDialog *dialog = prv_create_fatal_dialog(data);
  kernel_free(data);
  // Keep the warning above the safe watchface until it is dismissed.
  expandable_dialog_push(dialog, modal_manager_get_window_stack(ModalPriorityAlert));
  watchface_set_default_install_id(INSTALL_ID_INVALID);
  watchface_launch_default(nullptr);
}

void moddable_cleanup(void) {
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

  app_state_set_js_memory_api_context(nullptr);
  task_free(state);
}

typedef struct {
  txBuildFFI build_ffi;
  txMachine *the;
  txAPI *api;
} BuildFFICall;

static txBuildFFI s_app_build_ffi;

static void prv_build_ffi_unprivileged_cb(void *data) {
  BuildFFICall *call = data;
  call->build_ffi(call->the, call->api);
}

static void prv_build_ffi(txMachine *the, txAPI *api) {
  BuildFFICall call = {
    .build_ffi = s_app_build_ffi,
    .the = the,
    .api = api,
  };
  mcu_call_unprivileged(prv_build_ffi_unprivileged_cb, &call);
}

// Minimum recordSize for the original struct (without flags field)
#define kModdableCreationRecordMinSize offsetof(ModdableCreationRecord, flags)
#define kModdableCreationRecordFlagsSize \
  (offsetof(ModdableCreationRecord, flags) + sizeof(((ModdableCreationRecord *)0)->flags))
#define kModdableCreationRecordFFISize \
  (offsetof(ModdableCreationRecord, fxBuildFFI) + sizeof(((ModdableCreationRecord *)0)->fxBuildFFI))

static void prv_assert_userspace_creation_record(bool from_user, ModdableCreationRecord *cr,
                                                 size_t len) {
  if (from_user) {
    syscall_assert_userspace_buffer(cr, len);
  }
}

PBL_SYSCALL(void, moddable_createMachine, (VAL, ModdableCreationRecord *, cr)) {
  uint32_t flags = 0;
  uint32_t record_size = 0;

  if (from_user) {
    const PebbleProcessMd *md = app_manager_get_current_app_md();
    if (pebble_task_get_current() != PebbleTask_App || md == nullptr || !md->is_moddable_app ||
        app_state_get_js_memory_api_context() != nullptr) {
      syscall_failed();
    }
  }

  ModdablePebbleAppState state = task_zalloc_check(sizeof(ModdablePebbleAppStateRecord));
  app_state_set_js_memory_api_context((void *)state);

  // Read flags if the record is large enough to include them
  if (cr) {
    prv_assert_userspace_creation_record(from_user, cr, sizeof(cr->recordSize));
    record_size = cr->recordSize;
    if (record_size < kModdableCreationRecordMinSize) {
      APP_LOG(APP_LOG_LEVEL_ERROR, "invalid recordSize");
      return;
    }

    prv_assert_userspace_creation_record(from_user, cr, record_size);
    if (record_size >= kModdableCreationRecordFlagsSize)
      flags = cr->flags;
  }

  // Don't log instrumentation if nobody is listening to APP_LOG over BT
  if (!app_log_is_bt_enabled())
    flags &= ~(kModdableCreationFlagLogInstrumentation | kModdableCreationFlagDebug);

  state->creationFlags = flags;

  void *fxBuildFFI = nullptr;
  xsCreation *defaultCreation;
  extern void *xsPreparationAndCreation(xsCreation * *creation);
  (void)xsPreparationAndCreation(&defaultCreation);
  struct xsCreationRecord creation = *defaultCreation;
  if (nullptr != cr) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "evaluating creation record");
    uint32_t stack = (cr->stack + 3) & ~3, slot = (cr->slot + 3) & ~3, chunk = (cr->chunk + 3) & ~3;
    if (stack || slot || chunk) {
      if (!stack || !slot || !chunk) {
        APP_LOG(APP_LOG_LEVEL_ERROR, "invalid ModdableCreationRecord");
        return;
      }

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

    if (record_size >= kModdableCreationRecordFFISize) {
      fxBuildFFI = cr->fxBuildFFI;

      if (fxBuildFFI && creation.staticSize) {
        int available = creation.staticSize - (creation.stackCount * sizeof(xsSlot));
        creation.initialHeapCount = (available >> 1) / sizeof(xsSlot);
        creation.initialChunkSize = available >> 1;
        creation.staticSize = 0;
      }
    }
  }

  xsMachine *the = modCloneMachine(&creation, nullptr);
  if (nullptr == the) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "failed to allocate XS machine");
    moddable_cleanup();
    return;
  }

  state->the = the;
  s_app_build_ffi = (txBuildFFI)fxBuildFFI;
  state->fxBuildFFI = fxBuildFFI ? (void *)prv_build_ffi : nullptr;
  state->eventedTimer = EVENTED_TIMER_INVALID_ID;

  evented_timer_register(1, false, (EventedTimerCallback)modRunMachineSetup, the);

  xsBeginHostExit(the);
  app_event_loop();
  xsEndHostExit(the);

  int exitStatus = the->exitStatus;
  char *abortReason = state->abortReason;
  state->abortReason = nullptr;
  moddable_cleanup();

  if ((xsNormalExit != exitStatus) && (xsDebuggerExit != exitStatus)) {
    const char *message = abortReason ? abortReason : fxAbortString(exitStatus);
    if (sys_app_is_watchface()) {
      char *warning = kernel_strdup_check(message);
      if (abortReason)
        c_free(abortReason);
      launcher_task_add_callback(prv_watchface_fatal_error, warning);
      app_event_loop();
      return;
    }

    ExpandableDialog *dialog = prv_create_fatal_dialog(message);
    if (abortReason)
      c_free(abortReason);

    app_expandable_dialog_push(dialog);

    app_event_loop();
  }
}

#else

PBL_SYSCALL(void, moddable_createMachine, (VAL, ModdableCreationRecord *, cr)) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Moddable XS not supported in this build");
}

// Normally provided by the moddable submodule (xsPlatform.c). The xsbug
// endpoint stays in the protocol endpoints table regardless of build config,
// so stub the callback when building without moddable to satisfy the linker.
struct CommSession;

void xsbug_protocol_msg_callback(struct CommSession *session, const uint8_t *msg, size_t length) {
}
#endif
