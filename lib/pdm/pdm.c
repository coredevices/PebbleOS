/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pdm_tables.h"

#include <errno.h>
#include <string.h>

#include <pbl/pdm/pdm.h>
#include <pbl/util/math.h>
#include <pbl/util/size.h>

static_assert(SINC_BYTES == PBL_PDM_SINC_BYTES, "sinc span");
static_assert(PBL_ARRAY_SIZE(s_fir16) <= PBL_PDM_MAX_TAPS, "taps");
static_assert(PBL_ARRAY_SIZE(s_fir8) <= PBL_PDM_MAX_TAPS, "taps");

// Decimation of the sinc stage
#define SINC_DECIMATION 8U

// High-pass pole at 1 - 2^-HP_SHIFT, 20 Hz at 16 kHz; its state in Q(HP_FRAC)
#define HP_SHIFT 7
#define HP_FRAC  8

// Gain in Q(GAIN_FRAC): 10^(dB / 20) for 0 to 5 dB, doubled every 6 dB
#define GAIN_FRAC 12
static const int32_t s_gain_db[] = {4096, 4596, 5157, 5786, 6492, 7284};

int pbl_pdm_filter_init(struct pbl_pdm_filter *filter, uint32_t decimation, uint32_t settle) {
  memset(filter, 0, sizeof(*filter));

  switch (decimation) {
    case 64U:
      filter->taps = s_fir8;
      filter->num_taps = PBL_ARRAY_SIZE(s_fir8);
      filter->shift = FIR8_SHIFT;
      break;
    case 128U:
      filter->taps = s_fir16;
      filter->num_taps = PBL_ARRAY_SIZE(s_fir16);
      filter->shift = FIR16_SHIFT;
      break;
    default:
      return -EINVAL;
  }

  filter->decimation = (uint8_t)(decimation / SINC_DECIMATION);
  filter->silent = PBL_DIV_ROUND_UP(filter->num_taps, filter->decimation) + settle;
  // Silence, alternating ones and zeros
  memset(filter->bytes, 0x55, sizeof(filter->bytes));
  filter->gain = s_gain_db[0];

  return 0;
}

void pbl_pdm_filter_set_gain(struct pbl_pdm_filter *filter, int32_t gain_mdb) {
  int32_t db = (PBL_CLAMP(gain_mdb, 0, PBL_PDM_GAIN_MAX_MDB) + 500) / 1000;

  filter->gain = s_gain_db[db % 6] << (db / 6);
}

static int16_t prv_sinc(struct pbl_pdm_filter *filter, uint8_t byte) {
  uint8_t pos = filter->byte_pos;
  int32_t acc = 0;

  filter->bytes[pos] = byte;
  for (uint8_t i = 0U; i < SINC_BYTES; i++) {
    acc += s_sinc[i][filter->bytes[pos]];
    pos = (pos == 0U) ? SINC_BYTES - 1U : pos - 1U;
  }
  filter->byte_pos = (filter->byte_pos + 1U) % SINC_BYTES;

  return (int16_t)PBL_CLAMP(acc, -INT16_MAX, INT16_MAX);
}

static int32_t prv_fir(const struct pbl_pdm_filter *filter) {
  const int16_t *hist = &filter->hist[filter->pos];
  int64_t acc = 0;

  for (uint16_t i = 0U; i < filter->num_taps; i++) {
    acc += (int32_t)hist[i] * filter->taps[i];
  }

  return (int32_t)(acc >> filter->shift);
}

static int16_t prv_output(struct pbl_pdm_filter *filter, int32_t x) {
  int32_t xq = x * (1 << HP_FRAC);
  int32_t y;
  int64_t out;

  // Silence until the taps only see the settled stream, its offset the state of the high-pass
  if (filter->silent > 0U) {
    filter->silent--;
    filter->hp_x = xq;
    return 0;
  }

  y = xq - filter->hp_x + filter->hp_y - (filter->hp_y >> HP_SHIFT);

  filter->hp_x = xq;
  filter->hp_y = y;

  out = ((int64_t)(y >> HP_FRAC) * filter->gain) >> GAIN_FRAC;

  return (int16_t)PBL_CLAMP(out, INT16_MIN, INT16_MAX);
}

size_t pbl_pdm_filter_run(struct pbl_pdm_filter *filter, const uint8_t *in, size_t len,
                          size_t in_stride, int16_t *out, size_t out_stride) {
  size_t n = 0U;

  for (size_t i = 0U; i < len; i++) {
    int16_t v = prv_sinc(filter, in[i * in_stride]);

    // Each sample twice, for the taps to read the last ones in one run
    filter->hist[filter->pos] = v;
    filter->hist[filter->pos + filter->num_taps] = v;
    filter->pos = (filter->pos + 1U) % filter->num_taps;

    if (++filter->phase < filter->decimation) {
      continue;
    }
    filter->phase = 0U;

    out[n * out_stride] = prv_output(filter, prv_fir(filter));
    n++;
  }

  return n;
}
