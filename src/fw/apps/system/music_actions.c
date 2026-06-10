/* SPDX-License-Identifier: Apache-2.0 */

#include "music_actions.h"

#include "applib/app_exit_reason.h"
#include "pbl/services/i18n/i18n.h"
#include "pbl/services/music.h"
#include "process_management/app_manager.h"

static void prv_send_music_command(MusicCommand command) {
  music_command_send(command);
  app_exit_reason_set(APP_EXIT_ACTION_PERFORMED_SUCCESSFULLY);
}

static void prv_next_track_main(void) {
  prv_send_music_command(MusicCommandNextTrack);
}

static void prv_previous_track_main(void) {
  prv_send_music_command(MusicCommandPreviousTrack);
}

static void prv_play_pause_main(void) {
  prv_send_music_command(MusicCommandTogglePlayPause);
}

static void prv_volume_up_main(void) {
  prv_send_music_command(MusicCommandVolumeUp);
}

static void prv_volume_down_main(void) {
  prv_send_music_command(MusicCommandVolumeDown);
}

const PebbleProcessMd *music_next_track_action_get_app_info(void) {
  static const PebbleProcessMdSystem s_app_info = {
      .common =
          {
              .main_func = &prv_next_track_main,
              .uuid = MUSIC_NEXT_TRACK_ACTION_UUID,
              .visibility = ProcessVisibilityQuickLaunch,
          },
      .name = i18n_noop("Next Track"),
  };
  return &s_app_info.common;
}

const PebbleProcessMd *music_previous_track_action_get_app_info(void) {
  static const PebbleProcessMdSystem s_app_info = {
      .common =
          {
              .main_func = &prv_previous_track_main,
              .uuid = MUSIC_PREVIOUS_TRACK_ACTION_UUID,
              .visibility = ProcessVisibilityQuickLaunch,
          },
      .name = i18n_noop("Previous Track"),
  };
  return &s_app_info.common;
}

const PebbleProcessMd *music_play_pause_action_get_app_info(void) {
  static const PebbleProcessMdSystem s_app_info = {
      .common =
          {
              .main_func = &prv_play_pause_main,
              .uuid = MUSIC_PLAY_PAUSE_ACTION_UUID,
              .visibility = ProcessVisibilityQuickLaunch,
          },
      .name = i18n_noop("Play/Pause"),
  };
  return &s_app_info.common;
}

const PebbleProcessMd *music_volume_up_action_get_app_info(void) {
  static const PebbleProcessMdSystem s_app_info = {
      .common =
          {
              .main_func = &prv_volume_up_main,
              .uuid = MUSIC_VOLUME_UP_ACTION_UUID,
              .visibility = ProcessVisibilityQuickLaunch,
          },
      .name = i18n_noop("Volume Up"),
  };
  return &s_app_info.common;
}

const PebbleProcessMd *music_volume_down_action_get_app_info(void) {
  static const PebbleProcessMdSystem s_app_info = {
      .common =
          {
              .main_func = &prv_volume_down_main,
              .uuid = MUSIC_VOLUME_DOWN_ACTION_UUID,
              .visibility = ProcessVisibilityQuickLaunch,
          },
      .name = i18n_noop("Volume Down"),
  };
  return &s_app_info.common;
}
