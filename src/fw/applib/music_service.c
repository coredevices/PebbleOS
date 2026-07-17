/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "music_service.h"
#include "music_service_private.h"

#include "event_service_client.h"
#include "kernel/events.h"
#include "syscall/syscall.h"
#include "system/passert.h"

#include "process_state/app_state/app_state.h"
#include "process_state/worker_state/worker_state.h"


// ----------------------------------------------------------------------------------------------------
static MusicServiceState* prv_get_state(PebbleTask task) {
  if (task == PebbleTask_Unknown) {
    task = pebble_task_get_current();
  }

  if (task == PebbleTask_App) {
    return app_state_get_music_service_state();
  } else if (task == PebbleTask_Worker) {
    return worker_state_get_music_service_state();
  } else {
    WTF;
  }
}


static void do_handle(PebbleEvent *e, void *context) {
  MusicServiceState *state = prv_get_state(PebbleTask_Unknown);
  PBL_ASSERTN(state->handler != NULL);
  switch (e->media.type) {
    case PebbleMediaEventTypeNowPlayingChanged:
    case PebbleMediaEventTypeServerConnected:
    case PebbleMediaEventTypeServerDisconnected:
      state->handler(MusicServiceEventNowPlayingChanged);
      break;
    case PebbleMediaEventTypePlaybackStateChanged:
      state->handler(MusicServiceEventPlaybackStateChanged);
      break;
    default:
      // Volume / track position changes are not exposed through this API
      break;
  }
}

bool music_service_has_now_playing(void) {
  return sys_music_has_now_playing();
}

void music_service_get_now_playing(char *title, char *artist, char *album) {
  sys_music_get_now_playing(title, artist, album);
}

MusicServicePlaybackState music_service_get_playback_state(void) {
  switch (sys_music_get_playback_state()) {
    case MusicPlayStatePlaying:
      return MusicServicePlaybackStatePlaying;
    case MusicPlayStatePaused:
      return MusicServicePlaybackStatePaused;
    case MusicPlayStateForwarding:
      return MusicServicePlaybackStateForwarding;
    case MusicPlayStateRewinding:
      return MusicServicePlaybackStateRewinding;
    case MusicPlayStateUnknown:
    case MusicPlayStateInvalid:
    default:
      return MusicServicePlaybackStateUnknown;
  }
}

void music_service_subscribe(MusicServiceEventHandler handler) {
  MusicServiceState *state = prv_get_state(PebbleTask_Unknown);
  state->handler = handler;
  event_service_client_subscribe(&state->mss_info);
}

void music_service_unsubscribe(void) {
  MusicServiceState *state = prv_get_state(PebbleTask_Unknown);
  event_service_client_unsubscribe(&state->mss_info);
  state->handler = NULL;
}

void music_service_state_init(MusicServiceState *state) {
  *state = (MusicServiceState) {
    .mss_info = {
      .type = PEBBLE_MEDIA_EVENT,
      .handler = &do_handle,
    },
  };
}
