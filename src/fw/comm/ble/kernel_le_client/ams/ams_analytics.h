/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

typedef enum {
  AMSAnalyticsEventErrorReserved = 0,
  // Lifecycle
  AMSAnalyticsEventErrorDiscovery = 1,
  AMSAnalyticsEventErrorSubscribe = 2,
  AMSAnalyticsEventErrorMusicServiceConnect = 3,
  AMSAnalyticsEventErrorRegisterEntityWrite = 4,
  AMSAnalyticsEventErrorRegisterEntityWriteResponse = 5,
  AMSAnalyticsEventErrorOtherWriteResponse = 6,
  // Updates
  AMSAnalyticsEventErrorPlayerVolumeUpdate = 7,
  AMSAnalyticsEventErrorPlayerPlaybackInfoUpdate = 8,
  AMSAnalyticsEventErrorPlayerPlaybackInfoFloatParse = 9,
  AMSAnalyticsEventErrorTrackDurationUpdate = 10,
  // Sending Remote Commands
  AMSAnalyticsEventErrorSendRemoteCommand = 11,
} AMSAnalyticsEvent;
