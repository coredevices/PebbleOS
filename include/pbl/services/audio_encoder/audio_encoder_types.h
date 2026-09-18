/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

//! @addtogroup Foundation
//! @{
//!   @addtogroup AudioEncoder
//!   @{

//! Speech codecs. Availability depends on the firmware build; check with
//! \ref audio_encoder_codec_available.
typedef enum AudioCodec {
  AudioCodecInvalid = 0,
  //! Speex wideband, 16 kHz, ~9.8 kbps. Frames of 320 samples, ~30 byte packets.
  AudioCodecSpeexWB = 1,
  AudioCodecCount,
} AudioCodec;

//! Describes an open encoder. Everything an app needs to tell the phone how to decode.
typedef struct AudioEncoderInfo {
  uint8_t codec;             //!< AudioCodec
  uint8_t channels;          //!< Interleaved input channels (1 for the watch mic)
  uint16_t frame_samples;    //!< Input samples per channel per encode call
  uint32_t sample_rate;      //!< Hz
  uint32_t bitrate;          //!< bits per second (nominal)
  uint16_t max_packet_bytes; //!< Upper bound on one encoded frame
  uint8_t bitstream_version; //!< Codec-specific bitstream version
  uint8_t reserved;
} AudioEncoderInfo;

//!   @} // end addtogroup AudioEncoder
//! @} // end addtogroup Foundation
