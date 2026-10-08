/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/kernel/compiler.h>

/**
 * @addtogroup services_music_endpoint
 * @{
 */

/** @brief Music endpoint message identifier, the first byte of every message. */
typedef enum {
  /** Watch to phone: toggle play/pause. */
  MusicEndpointCmdIDTogglePlayPause = 0x1,
  /** Watch to phone: pause. */
  MusicEndpointCmdIDPause = 0x2,
  /** Watch to phone: play. */
  MusicEndpointCmdIDPlay = 0x3,
  /** Watch to phone: next track. */
  MusicEndpointCmdIDNextTrack = 0x4,
  /** Watch to phone: previous track. */
  MusicEndpointCmdIDPreviousTrack = 0x5,
  /** Watch to phone: volume up. */
  MusicEndpointCmdIDVolumeUp = 0x6,
  /** Watch to phone: volume down. */
  MusicEndpointCmdIDVolumeDown = 0x7,
  /** Watch to phone: request all the info responses. */
  MusicEndpointCmdIDGetAllInfo = 0x8,

  /** Phone to watch: artist, album and title, then optional extended fields. */
  MusicEndpointCmdIDNowPlayingInfoResponse = 0x10,
  /** Phone to watch: MusicEndpointPlayStateInfo. */
  MusicEndpointCmdIDPlayStateInfoResponse = 0x11,
  /** Phone to watch: volume percentage, one byte. */
  MusicEndpointCmdIDVolumeInfoResponse = 0x12,
  /** Phone to watch: player package and name. */
  MusicEndpointCmdIDPlayerInfoResponse = 0x13,

  /** No equivalent command. */
  MusicEndpointCmdIDInvalid = 0xff,
} MusicEndpointCmdID;

/** @brief Playback state on the wire. */
typedef enum {
  /** Paused. */
  MusicEndpointPlaybackStatePaused = 0,
  /** Playing. */
  MusicEndpointPlaybackStatePlaying = 1,
  /** Rewinding. */
  MusicEndpointPlaybackStateRewinding = 2,
  /** Fast-forwarding. */
  MusicEndpointPlaybackStateForwarding = 3,
  /** Unknown. */
  MusicEndpointPlaybackStateUnknown = 4,
} MusicEndpointPlaybackState;

/** @brief Shuffle mode on the wire. */
typedef enum {
  /** Unknown. */
  MusicEndpointShuffleModeUnknown = 0,
  /** Shuffle off. */
  MusicEndpointShuffleModeOff = 1,
  /** Shuffle on. */
  MusicEndpointShuffleModeOn = 2,
} MusicEndpointShuffleMode;

/** @brief Repeat mode on the wire. */
typedef enum {
  /** Unknown. */
  MusicEndpointRepeatModeUnknown = 0,
  /** Repeat off. */
  MusicEndpointRepeatModeOff = 1,
  /** Repeat the current track. */
  MusicEndpointRepeatModeOne = 2,
  /** Repeat all tracks. */
  MusicEndpointRepeatModeAll = 3,
} MusicEndpointRepeatMode;

/**
 * @brief Bits of the optional byte trailing MusicEndpointPlayStateInfo.
 *
 * Phone apps that predate it send the shorter message, which reads as no bits set.
 */
typedef enum {
  /** Next/previous track commands seek within the track. */
  MusicEndpointSkipSeeksWithinTrack = (1 << 0),
} MusicEndpointSkipSeeksFlag;

/**
 * @brief Payload of #MusicEndpointCmdIDPlayStateInfoResponse.
 *
 * An optional byte of MusicEndpointSkipSeeksFlag bits may follow; check the message length
 * before reading it.
 */
typedef struct PBL_PACKED {
  /** MusicEndpointPlaybackState. */
  uint8_t play_state;
  /** Track position in milliseconds, negative if progress is not reported. */
  int32_t track_pos_ms;
  /** Playback rate in percent. */
  int32_t play_rate;
  /** MusicEndpointShuffleMode, currently ignored. */
  uint8_t play_shuffle_mode;
  /** MusicEndpointRepeatMode, currently ignored. */
  uint8_t play_repeat_mode;
} MusicEndpointPlayStateInfo;

/** @} */
