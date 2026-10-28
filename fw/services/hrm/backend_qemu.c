/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <string.h>

#include <pbl/drivers/ppg/qemu.h>
#include <pbl/services/hrm/hrm_ppg.h>
#include <pbl/services/hrm/hrm_qemu.h>
#include <pbl/util/math.h>
#include <pbl/util/size.h>

#define SEC_PER_MIN             60
#define FRAC_SHIFT              8
#define BASELINE_SHIFT          5
#define BEATS                   4U
#define PERIOD_TOLERANCE_PCT    3
#define PERCENT                 100
#define PERFUSION_STEP_PERMILLE 5U
#define WEAR_MIN_PA             500000LL
/* A level change above 1/5 is a gain step, not a pulse. */
#define STEP_DIV 5

static const uint8_t s_confidence[] = {
  [HRMQuality_Worst] = 0U, [HRMQuality_Poor] = 70U,      [HRMQuality_Acceptable] = 80U,
  [HRMQuality_Good] = 90U, [HRMQuality_Excellent] = 98U,
};

static struct {
  const struct hrm_ppg_session *session;
  int8_t green;
  uint32_t frame;
  bool baseline_valid;
  int64_t baseline;
  int32_t prev_ac;
  bool beat_open;
  int64_t beat_start;
  int64_t beat_sum;
  uint32_t beat_count;
  int32_t beat_max;
  int32_t beat_min;
  int64_t periods[BEATS];
  uint32_t num_periods;
  int64_t dc;
  int32_t amplitude;
  uint32_t gain_ohm;
} s_qemu;

static int prv_start(const struct hrm_ppg_session *session) {
  memset(&s_qemu, 0, sizeof(s_qemu));
  s_qemu.session = session;
  s_qemu.green = -1;

  for (uint8_t i = 0U; i < session->num_channels; i++) {
    if (session->channels[i].role == HRM_PPG_ROLE_GREEN) {
      s_qemu.green = (int8_t)i;
      break;
    }
  }

  return 0;
}

static void prv_stop(void) {
  s_qemu.session = nullptr;
}

static void prv_end_beat(int64_t crossing) {
  if (s_qemu.beat_open && (s_qemu.beat_count > 0U)) {
    memmove(&s_qemu.periods[1], &s_qemu.periods[0], (BEATS - 1U) * sizeof(s_qemu.periods[0]));
    s_qemu.periods[0] = crossing - s_qemu.beat_start;
    s_qemu.num_periods = MIN(s_qemu.num_periods + 1U, BEATS);
    s_qemu.dc = s_qemu.beat_sum / s_qemu.beat_count;
    s_qemu.amplitude = (s_qemu.beat_max - s_qemu.beat_min) / 2;
  }

  s_qemu.beat_open = true;
  s_qemu.beat_start = crossing;
  s_qemu.beat_sum = 0;
  s_qemu.beat_count = 0U;
  s_qemu.beat_max = INT32_MIN;
  s_qemu.beat_min = INT32_MAX;
}

static bool prv_periods_stable(int64_t *mean) {
  int64_t sum = 0;

  if (s_qemu.num_periods < BEATS) {
    return false;
  }

  for (uint32_t i = 0U; i < BEATS; i++) {
    sum += s_qemu.periods[i];
  }

  *mean = sum / BEATS;

  for (uint32_t i = 0U; i < BEATS; i++) {
    if ((PERCENT * ABS(s_qemu.periods[i] - *mean)) > (PERIOD_TOLERANCE_PCT * *mean)) {
      return false;
    }
  }

  return *mean > 0;
}

static void prv_report(void) {
  const struct hrm_ppg_session *session = s_qemu.session;
  struct pbl_ppg_channel channel = {.calibrated_gain_ohm = s_qemu.gain_ohm};
  int64_t level = s_qemu.baseline_valid ? (s_qemu.baseline >> BASELINE_SHIFT) : 0;
  int64_t period;
  uint32_t perfusion;
  uint32_t step;

  if (pbl_ppg_value_to_pa(session->info, &channel, (int32_t)level) < WEAR_MIN_PA) {
    hrm_ppg_report_wear(false);
    hrm_ppg_report_bpm(0U, 0U);
    return;
  }

  if (!prv_periods_stable(&period) || (s_qemu.dc <= 0)) {
    return;
  }

  hrm_ppg_report_wear(true);

  perfusion = (uint32_t)(((int64_t)s_qemu.amplitude * 2 * 1000) / s_qemu.dc);
  step = (perfusion + PERFUSION_STEP_PERMILLE / 2U) / PERFUSION_STEP_PERMILLE;
  step = CLIP(step, 1U, ARRAY_LENGTH(s_confidence));

  hrm_ppg_report_bpm(
      (uint8_t)((((int64_t)SEC_PER_MIN * session->sample_rate_hz << FRAC_SHIFT) + period / 2) /
                period),
      s_confidence[step - 1U]);
}

static void prv_process(const struct hrm_ppg_frame *frame) {
  int32_t value;
  int32_t ac;

  if ((s_qemu.session == nullptr) || (s_qemu.green < 0)) {
    return;
  }

  value = frame->value[s_qemu.green];
  s_qemu.gain_ohm = frame->gain_ohm[s_qemu.green];

  if (s_qemu.baseline_valid) {
    int64_t level = s_qemu.baseline >> BASELINE_SHIFT;

    if ((STEP_DIV * ABS(value - level)) > ABS(level)) {
      s_qemu.baseline_valid = false;
      s_qemu.beat_open = false;
      s_qemu.num_periods = 0U;
      s_qemu.prev_ac = 0;
    }
  }

  if (!s_qemu.baseline_valid) {
    s_qemu.baseline = (int64_t)value << BASELINE_SHIFT;
    s_qemu.baseline_valid = true;
  }

  s_qemu.baseline += value - (s_qemu.baseline >> BASELINE_SHIFT);
  ac = value - (int32_t)(s_qemu.baseline >> BASELINE_SHIFT);

  if ((s_qemu.prev_ac < 0) && (ac >= 0)) {
    int64_t frac = ((int64_t)(-s_qemu.prev_ac) << FRAC_SHIFT) / (ac - s_qemu.prev_ac);

    prv_end_beat(((int64_t)(s_qemu.frame - 1U) << FRAC_SHIFT) + frac);
  }

  s_qemu.prev_ac = ac;
  s_qemu.beat_sum += value;
  s_qemu.beat_count++;
  s_qemu.beat_max = MAX(s_qemu.beat_max, value);
  s_qemu.beat_min = MIN(s_qemu.beat_min, value);

  if ((++s_qemu.frame % s_qemu.session->sample_rate_hz) == 0U) {
    prv_report();
  }
}

const struct hrm_ppg_backend HRM_PPG_BACKEND = {
  .start = prv_start,
  .stop = prv_stop,
  .process = prv_process,
};

void hrm_qemu_set_heart_rate(uint8_t bpm, HRMQuality quality) {
  uint16_t perfusion = 0U;

  if (quality != HRMQuality_OffWrist) {
    perfusion = (uint16_t)((CLIP(quality, HRMQuality_Worst, HRMQuality_Excellent) + 1) *
                           PERFUSION_STEP_PERMILLE);
  }

  qemu_ppg_set_pulse(bpm, perfusion);
}
