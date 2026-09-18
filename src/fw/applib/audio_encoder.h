/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "pbl/services/audio_encoder/audio_encoder_types.h"

#include <stdbool.h>
#include <stdint.h>

//! @addtogroup Foundation
//! @{
//!   @addtogroup AudioEncoder
//! \brief Compressing microphone audio before sending it to the phone
//!
//! Raw 16 kHz PCM is far too much for the Bluetooth link, so an app that streams audio to its
//! phone-side component should compress it first. The firmware provides a speech encoder the app
//! can feed one frame at a time; the resulting packets are small enough to batch into
//! AppMessages. A typical loop:
//!
//! \code{.c}
//! AudioEncoderInfo info;
//! audio_encoder_open(AudioCodecSpeexWB, &info);
//! mic_data_service_subscribe(info.frame_samples, handlers, NULL);
//! // in the data handler:
//! uint8_t packet[info.max_packet_bytes];
//! int len = audio_encoder_encode_frame(samples, num_samples, packet, sizeof(packet));
//! // append packet to an AppMessage buffer and send every N frames
//! \endcode
//!
//! Send the \ref AudioEncoderInfo to the phone once so it can configure its decoder. Only one
//! encoder can be open at a time and the system may take it away for dictation, in which case
//! capture stops first (see \ref MicDataStopReasonInterrupted) and \ref audio_encoder_encode_frame
//! fails until the app opens it again.
//!   @{

//! @return true if this firmware can encode with the codec
bool audio_encoder_codec_available(AudioCodec codec);

//! Opens the encoder.
//! @param codec Codec to encode with
//! @param[out] info_out Frame size, rates and packet bound of the opened encoder
//! @return false if the codec is unavailable or the encoder is in use
bool audio_encoder_open(AudioCodec codec, AudioEncoderInfo *info_out);

//! Encodes one frame.
//! @param pcm Exactly `frame_samples * channels` samples from \ref AudioEncoderInfo
//! @param num_samples Number of samples in pcm
//! @param out Buffer of at least `max_packet_bytes`
//! @param out_len Size of out
//! @return Number of bytes written to out, or a negative value on error
int audio_encoder_encode_frame(const int16_t *pcm, uint32_t num_samples, uint8_t *out,
                               uint32_t out_len);

//! Closes the encoder. Safe to call when not open.
void audio_encoder_close(void);

//!   @} // end addtogroup AudioEncoder
//! @} // end addtogroup Foundation
