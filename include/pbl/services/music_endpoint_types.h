/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include "pbl/util/attributes.h"

typedef enum {
  // Watch -> Phone
  MusicEndpointCmdIDTogglePlayPause = 0x1,
  MusicEndpointCmdIDPause = 0x2,
  MusicEndpointCmdIDPlay = 0x3,
  MusicEndpointCmdIDNextTrack = 0x4,
  MusicEndpointCmdIDPreviousTrack = 0x5,
  MusicEndpointCmdIDVolumeUp = 0x6,
  MusicEndpointCmdIDVolumeDown = 0x7,
  MusicEndpointCmdIDGetAllInfo = 0x8,
  MusicEndpointCmdIDGetOutputRoutes = 0x9,
  MusicEndpointCmdIDSelectOutputRoute = 0xa,

  // Phone -> Watch
  MusicEndpointCmdIDNowPlayingInfoResponse = 0x10,
  MusicEndpointCmdIDPlayStateInfoResponse = 0x11,
  MusicEndpointCmdIDVolumeInfoResponse = 0x12,
  MusicEndpointCmdIDPlayerInfoResponse = 0x13,
  MusicEndpointCmdIDOutputRoutesResponse = 0x14,

  MusicEndpointCmdIDInvalid = 0xff,
} MusicEndpointCmdID;

typedef enum {
  MusicEndpointPlaybackStatePaused = 0,
  MusicEndpointPlaybackStatePlaying = 1,
  MusicEndpointPlaybackStateRewinding = 2,
  MusicEndpointPlaybackStateForwarding = 3,
  MusicEndpointPlaybackStateUnknown = 4,
} MusicEndpointPlaybackState;

typedef enum {
  MusicEndpointShuffleModeUnknown = 0,
  MusicEndpointShuffleModeOff = 1,
  MusicEndpointShuffleModeOn = 2,
} MusicEndpointShuffleMode;

typedef enum {
  MusicEndpointRepeatModeUnknown = 0,
  MusicEndpointRepeatModeOff = 1,
  MusicEndpointRepeatModeOne = 2,
  MusicEndpointRepeatModeAll = 3,
} MusicEndpointRepeatMode;

//! Bits of the optional byte trailing MusicEndpointPlayStateInfo. Phone apps that predate it send
//! the shorter message, which reads as "no bits set".
typedef enum {
  MusicEndpointSkipSeeksWithinTrack = (1 << 0),
} MusicEndpointSkipSeeksFlag;

typedef enum {
  MusicEndpointOutputRouteStatusAvailable = 0,
  MusicEndpointOutputRouteStatusUnsupported = 1,
  MusicEndpointOutputRouteStatusPermissionRequired = 2,
  MusicEndpointOutputRouteStatusNoPlayer = 3,
  MusicEndpointOutputRouteStatusError = 4,
} MusicEndpointOutputRouteStatus;

typedef enum {
  MusicEndpointOutputRouteSelected = (1 << 0),
} MusicEndpointOutputRouteFlag;

typedef struct PACKED {
  uint8_t play_state;
  int32_t track_pos_ms;
  int32_t play_rate;
  uint8_t play_shuffle_mode;
  uint8_t play_repeat_mode;
  //! Trailing MusicEndpointSkipSeeksFlag bits follow, but are optional: check the message length
  //! before reading them.
} MusicEndpointPlayStateInfo;
