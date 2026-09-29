/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pbl/services/mic_manager.h"

#include "board/board.h"
#include "pbl/kernel/mutex.h"
#include <pbl/logging/logging.h>

PBL_LOG_MODULE_DEFINE(service_mic_manager, CONFIG_SERVICE_MIC_MANAGER_LOG_LEVEL);

static PBL_MUTEX_DEFINE(s_lock);

typedef struct {
  MicClient owner;
  MicManagerPreemptedCb on_preempted;
  void *preempt_context;
  //! Set while dictation is taking the mic away from an app, so the app cannot slip back in
  //! between the preempt callback and the restart.
  bool preempting;
} MicManagerState;

static MicManagerState s_state;

void mic_manager_init(void) {
  s_state = (MicManagerState){};
}

static bool prv_start_locked(MicClient client, MicDataHandlerCB handler, void *context,
                             int16_t *buffer, size_t buffer_len, MicManagerPreemptedCb on_preempted,
                             void *preempt_context) {
  if (!mic_start(MIC, handler, context, buffer, buffer_len)) {
    PBL_LOG_WRN("Mic failed to start for client %u", client);
    return false;
  }
  s_state.owner = client;
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
    pbl_mutex_unlock(&s_lock);
    return false;
  }

  if (s_state.owner == MicClientNone) {
    const bool rv = prv_start_locked(client, handler, context, buffer, buffer_len, on_preempted,
                                     preempt_context);
    pbl_mutex_unlock(&s_lock);
    return rv;
  }

  if (client != MicClientVoiceDictation) {
    // Apps never preempt anyone
    PBL_LOG_DBG("Mic busy (owner %u), refusing client %u", s_state.owner, client);
    pbl_mutex_unlock(&s_lock);
    return false;
  }

  // Dictation takes the mic away from the app
  PBL_LOG_DBG("Dictation preempting mic owner %u", s_state.owner);
  const MicManagerPreemptedCb preempted_cb = s_state.on_preempted;
  void *preempted_ctx = s_state.preempt_context;
  mic_stop(MIC);
  s_state.owner = MicClientNone;
  s_state.on_preempted = NULL;
  s_state.preempt_context = NULL;
  s_state.preempting = true;
  pbl_mutex_unlock(&s_lock);

  if (preempted_cb) {
    preempted_cb(preempted_ctx);
  }

  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  s_state.preempting = false;
  const bool rv =
      prv_start_locked(client, handler, context, buffer, buffer_len, on_preempted, preempt_context);
  pbl_mutex_unlock(&s_lock);
  return rv;
}

void mic_manager_release(MicClient client) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  if ((client != MicClientNone) && (s_state.owner == client)) {
    mic_stop(MIC);
    s_state.owner = MicClientNone;
    s_state.on_preempted = NULL;
    s_state.preempt_context = NULL;
    PBL_LOG_DBG("Mic released by client %u", client);
  }
  pbl_mutex_unlock(&s_lock);
}

MicClient mic_manager_get_owner(void) {
  return s_state.owner;
}
