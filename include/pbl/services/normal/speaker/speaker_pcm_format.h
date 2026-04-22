/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

//! PCM audio format for speaker streaming.
//! Bit layout: bit0 = sample rate (0=8kHz, 1=16kHz), bit1 = bit depth (0=8-bit, 1=16-bit).
//! All formats are mono signed PCM (8-bit samples are signed [-128,127], not unsigned).
typedef enum {
  SpeakerPcmFormat_8kHz_8bit = 0,    //!< 8kHz 8-bit signed (1 byte/sample)
  SpeakerPcmFormat_16kHz_8bit = 1,   //!< 16kHz 8-bit signed (1 byte/sample)
  SpeakerPcmFormat_8kHz_16bit = 2,   //!< 8kHz 16-bit signed little-endian (2 bytes/sample)
  SpeakerPcmFormat_16kHz_16bit = 3,  //!< 16kHz 16-bit signed little-endian (2 bytes/sample)
  SpeakerPcmFormatCount
} SpeakerPcmFormat;
