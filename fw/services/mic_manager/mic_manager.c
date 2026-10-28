/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "mic_source.h"

#include <pbl/kernel/mutex.h>
#include <pbl/logging/logging.h>
#include <pbl/services/mic_manager.h>

#include <kernel/event_loop.h>
#include <kernel/ui/modals/modal_manager.h>
#include <popups/mic_banner.h>
#include <process_management/app_manager.h>

PBL_LOG_MODULE_DEFINE(service_mic_manager, CONFIG_SERVICE_MIC_MANAGER_LOG_LEVEL);

static PBL_MUTEX_DEFINE(s_lock);

typedef struct {
  MicClient owner;
  //! App client holding the mic for a start that is still pending
  MicClient reserved;
  MicManagerPreemptedCb on_preempted;
  void *preempt_context;
  //! Set while dictation is taking the mic away from an app, so the app cannot slip back in
  //! between the preempt callback and the restart.
  bool preempting;
  bool dictation_active;
  bool banner_dirty;  //!< Owner changed to or from an app since the last post
  bool banner_queued; //!< A banner sync is waiting on KernelMain
} MicManagerState;

static MicManagerState s_state;

void mic_manager_init(void) {
  s_state = (MicManagerState){};
}

static bool prv_is_app(MicClient client) {
  return (client != MicClientNone) && (client != MicClientVoiceDictation);
}

// The banner is UI, so it follows the owner from KernelMain whichever task changed it.
static void prv_sync_banner(void *unused) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  s_state.banner_queued = false;
  const bool show = prv_is_app(s_state.owner);
  pbl_mutex_unlock(&s_lock);
  if (show) {
    mic_banner_show();
  } else {
    mic_banner_hide();
  }
}

static void prv_set_owner_locked(MicClient client) {
  if (prv_is_app(s_state.owner) || prv_is_app(client)) {
    s_state.banner_dirty = true;
  }
  s_state.owner = client;
}

//! Posts the banner sync only once unlocked: event_put() can block while KernelMain waits on us.
static void prv_unlock(void) {
  const bool post = s_state.banner_dirty && !s_state.banner_queued;
  s_state.banner_dirty = false;
  s_state.banner_queued |= post;
  pbl_mutex_unlock(&s_lock);
  if (post) {
    launcher_task_add_callback(prv_sync_banner, nullptr);
  }
}

static bool prv_start_locked(MicClient client, MicDataHandlerCB handler, void *context,
                             int16_t *buffer, size_t buffer_len, MicManagerPreemptedCb on_preempted,
                             void *preempt_context) {
  if (!mic_source_start(handler, context, buffer, buffer_len)) {
    PBL_LOG_WRN("Mic failed to start for client %u", client);
    return false;
  }
  prv_set_owner_locked(client);
  if (s_state.reserved == client) {
    s_state.reserved = MicClientNone;
  }
  s_state.on_preempted = on_preempted;
  s_state.preempt_context = preempt_context;
  PBL_LOG_DBG("Mic acquired by client %u", client);
  return true;
}

bool mic_manager_acquire(MicClient client, MicDataHandlerCB handler, void *context, int16_t *buffer,
                         size_t buffer_len, MicManagerPreemptedCb on_preempted,
                         void *preempt_context) {
  if ((client == MicClientNone) || !handler || !buffer || (buffer_len == 0)) {
    return false;
  }

  pbl_mutex_lock(&s_lock, PBL_FOREVER);

  if (s_state.preempting || (s_state.owner == client)) {
    prv_unlock();
    return false;
  }

  const bool held_for_other =
      ((s_state.reserved != MicClientNone) && (s_state.reserved != client)) ||
      s_state.dictation_active;
  if ((s_state.owner == MicClientNone) &&
      (!held_for_other || (client == MicClientVoiceDictation))) {
    const bool rv = prv_start_locked(client, handler, context, buffer, buffer_len, on_preempted,
                                     preempt_context);
    prv_unlock();
    return rv;
  }

  if (client != MicClientVoiceDictation) {
    // Apps never preempt anyone
    PBL_LOG_DBG("Mic busy (owner %u, reserved %u), refusing client %u", s_state.owner,
                s_state.reserved, client);
    prv_unlock();
    return false;
  }

  // Dictation takes the mic away from the app
  PBL_LOG_DBG("Dictation preempting mic owner %u", s_state.owner);
  const MicManagerPreemptedCb preempted_cb = s_state.on_preempted;
  void *preempted_ctx = s_state.preempt_context;
  mic_source_stop();
  prv_set_owner_locked(MicClientNone);
  s_state.on_preempted = nullptr;
  s_state.preempt_context = nullptr;
  s_state.preempting = true;
  prv_unlock();

  if (preempted_cb) {
    preempted_cb(preempted_ctx);
  }

  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  s_state.preempting = false;
  const bool rv =
      prv_start_locked(client, handler, context, buffer, buffer_len, on_preempted, preempt_context);
  prv_unlock();
  return rv;
}

bool mic_manager_reserve(MicClient client) {
  if (!prv_is_app(client)) {
    return false;
  }
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  const bool rv = !s_state.preempting && !s_state.dictation_active &&
                  (s_state.owner == MicClientNone) && (s_state.reserved == MicClientNone);
  if (rv) {
    s_state.reserved = client;
  }
  prv_unlock();
  return rv;
}

void mic_manager_release(MicClient client) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  if ((client != MicClientNone) && (s_state.reserved == client)) {
    s_state.reserved = MicClientNone;
  }
  if ((client != MicClientNone) && (s_state.owner == client)) {
    mic_source_stop();
    prv_set_owner_locked(MicClientNone);
    s_state.on_preempted = nullptr;
    s_state.preempt_context = nullptr;
    PBL_LOG_DBG("Mic released by client %u", client);
  }
  prv_unlock();
}

void mic_manager_set_dictation_active(bool active) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  s_state.dictation_active = active;
  prv_unlock();
}

MicClient mic_manager_get_owner(void) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  const MicClient owner = s_state.owner;
  prv_unlock();
  return owner;
}

bool mic_manager_is_busy(void) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  const bool busy = (s_state.owner != MicClientNone) || (s_state.reserved != MicClientNone) ||
                    s_state.dictation_active;
  prv_unlock();
  return busy;
}

MicManagerAppCheck mic_manager_check_app(PebbleTask task) {
  if (task != PebbleTask_App) {
    return MicManagerAppNotForeground;
  }
  if (app_manager_is_watchface_running()) {
    return MicManagerAppWatchface;
  }
  if (modal_manager_get_enabled() && !(modal_manager_get_properties() & ModalProperty_Unfocused)) {
    return MicManagerAppNotForeground;
  }
  if (!app_manager_get_current_app_md()) {
    return MicManagerAppNotForeground;
  }
  return MicManagerAppOk;
}
