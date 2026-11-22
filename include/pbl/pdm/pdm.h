/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>
#include <stdint.h>

/**
 * @defgroup pdm PDM to PCM
 * @ingroup lib
 * @brief Decimation of the 1-bit stream of a PDM microphone to 16-bit PCM.
 *
 * Bytes of eight PDM samples go through a fifth-order sinc filter decimating by 8, then an
 * equiripple low-pass FIR decimating by 8 or 16, flat to 0.4 times the output rate and at least
 * 70 dB down from 0.6 times it. A high-pass filter at 1/800 of the output rate, 20 Hz at 16 kHz,
 * then removes the offset of the microphone and its drift as it starts, and a gain of 0 to @ref
 * PBL_PDM_GAIN_MAX_MDB brings speech up from the low levels microphones give it. Without gain, a
 * stream of only ones gives full scale.
 *
 * One filter runs one microphone. The oldest of the eight samples of a byte is its most
 * significant bit. The first samples are silence: until the FIR has seen as many as it has
 * taps, and for the time the microphone takes to settle once clocked.
 *
 * @code{.c}
 * static struct pbl_pdm_filter s_filter;
 *
 * pbl_pdm_filter_init(&s_filter, 64, 0);
 * pbl_pdm_filter_set_gain(&s_filter, 24000);
 * // 128 bytes of a 1.024 MHz PDM clock, 1 ms, give 16 samples at 16 kHz
 * n = pbl_pdm_filter_run(&s_filter, pdm, 128, 1, pcm, 1);
 * @endcode
 * @{
 */

/** @brief Highest gain, in mdB. */
#define PBL_PDM_GAIN_MAX_MDB 42000

/** @cond INTERNAL_HIDDEN */
#define PBL_PDM_SINC_BYTES 5U
#define PBL_PDM_MAX_TAPS   256U
/** @endcond */

/** @brief Filter of one microphone. */
struct pbl_pdm_filter {
  /** @cond INTERNAL_HIDDEN */
  const int16_t *taps;
  uint16_t num_taps;
  uint8_t shift;
  uint8_t decimation;
  uint8_t phase;
  uint8_t byte_pos;
  uint16_t pos;
  uint32_t silent;
  uint8_t bytes[PBL_PDM_SINC_BYTES];
  int16_t hist[2U * PBL_PDM_MAX_TAPS];
  int32_t hp_x;
  int32_t hp_y;
  int32_t gain;
  /** @endcond */
};

/**
 * @brief Initialize a filter, without gain.
 *
 * @param filter Filter.
 * @param decimation PDM samples per output sample: 64 or 128.
 * @param settle Output samples to silence while the microphone settles.
 * @return 0, or @c -EINVAL for another decimation.
 */
int pbl_pdm_filter_init(struct pbl_pdm_filter *filter, uint32_t decimation, uint32_t settle);

/**
 * @brief Set the gain.
 *
 * @param filter Filter.
 * @param gain_mdb Gain in mdB, rounded to the dB and clamped to 0 to @ref PBL_PDM_GAIN_MAX_MDB.
 */
void pbl_pdm_filter_set_gain(struct pbl_pdm_filter *filter, int32_t gain_mdb);

/**
 * @brief Filter PDM bytes.
 *
 * @param filter Filter.
 * @param in First byte of PDM samples.
 * @param len Number of bytes.
 * @param in_stride Distance between bytes, in bytes, as for the bytes of one microphone of
 *        several interleaved.
 * @param out Where to write the samples.
 * @param out_stride Distance between samples, in samples, as for one channel of several.
 * @return Number of samples written: @p len times 8 divided by the decimation, give or take one
 *         for the bytes the previous call left over.
 */
size_t pbl_pdm_filter_run(struct pbl_pdm_filter *filter, const uint8_t *in, size_t len,
                          size_t in_stride, int16_t *out, size_t out_stride);

/** @} */
