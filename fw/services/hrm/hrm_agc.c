/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "hrm_agc.h"

#include <errno.h>
#include <inttypes.h>

#include <pbl/kernel/time.h>
#include <pbl/logging/logging.h>
#include <pbl/util/math.h>

PBL_LOG_MODULE_DECLARE(service_hrm, CONFIG_SERVICE_HRM_LOG_LEVEL);

/* Levels are codes of a 24-bit converter with its zero at mid-scale, scaled to the front end. */
#define Q23_SHIFT        23
#define Q23_ZERO         (1LL << Q23_SHIFT)
#define LEVEL(scale_pct) ((Q23_ZERO * (scale_pct)) / 100)

/* Receiver nonlinearity correction near full scale, in codes above zero. */
#define STRETCH_KNEE1 8000000LL
#define STRETCH_KNEE2 8100000LL
#define STRETCH_GAIN1 2LL
#define STRETCH_GAIN2 4LL

#define NANO_PER_MICRO 1000LL

/* Samples dropped after a change, as a fraction of the sample rate. */
#define SETTLE_DIV 5U
/* Saturation check window, in seconds, and the fraction of it that must be saturated. */
#define SAT_WINDOW_S     5U
#define SAT_FRACTION_DIV 16U
/* Re-centering: tolerance in eighths, stability in tenths, windows needed after the first. */
#define RECENTER_LOW     7
#define RECENTER_HIGH    9
#define RECENTER_DEN     8
#define STABLE_MAX       10
#define STABLE_MIN       12
#define RECENTER_WINDOWS 15U
/* Gain raises keep the projected maximum below 7/8 of the high level. */
#define HEADROOM_NUM 7
#define HEADROOM_DEN 8

struct agc_levels {
  int64_t high;
  int64_t low;
  int64_t ideal;
  int64_t ideal_na;
  int64_t high_na;
  int64_t low_na;
};

static const struct agc_levels s_levels[] = {
  [HRM_PPG_AGC_GENERAL] =
      {
        .high = LEVEL(185),
        .low = LEVEL(130),
        .ideal = LEVEL(156),
        .ideal_na = 2000,
        .high_na = 3060,
        .low_na = 1000,
      },
  [HRM_PPG_AGC_SPO2] = {
    .high = LEVEL(190),
    .low = LEVEL(150),
    .ideal = LEVEL(170),
    .ideal_na = 20000,
    .high_na = 25000,
    .low_na = 15000,
  },
};

static const struct agc_levels *prv_levels(const struct hrm_agc_slot *agc) {
  return &s_levels[agc->config->agc_target];
}

static int64_t prv_raw(const struct pbl_ppg_info *info, int32_t value) {
  return ((int64_t)value * Q23_ZERO) / info->full_scale + Q23_ZERO;
}

static int64_t prv_photocurrent_na(const struct pbl_ppg_info *info, uint32_t gain_ohm,
                                   int64_t mean) {
  int64_t off = mean - Q23_ZERO;

  if (off <= 0) {
    off = 1;
  }

  if (off > STRETCH_KNEE2) {
    off = STRETCH_KNEE1 + STRETCH_GAIN1 * (STRETCH_KNEE2 - STRETCH_KNEE1) +
          STRETCH_GAIN2 * (off - STRETCH_KNEE2);
  } else if (off > STRETCH_KNEE1) {
    off = STRETCH_KNEE1 + STRETCH_GAIN1 * (off - STRETCH_KNEE1);
  }

  return MAX((off * info->full_scale_uv * NANO_PER_MICRO) / (Q23_ZERO * gain_ohm), 1LL);
}

static uint8_t prv_gain_idx(const struct pbl_ppg_info *info, uint32_t ohm) {
  uint8_t idx = 0U;

  while (((idx + 1U) < info->num_gains) && (info->gains_ohm[idx + 1U] <= ohm)) {
    idx++;
  }

  return idx;
}

static void prv_window_reset(struct hrm_agc_window *w, uint32_t drop) {
  *w = (struct hrm_agc_window){
    .drop = drop,
    .max = INT32_MIN,
    .min = INT32_MAX,
  };
}

static int prv_apply(struct hrm_agc_slot *agc, const struct pbl_ppg *ppg, uint32_t led_ua,
                     const uint8_t *gain_idx) {
  const struct pbl_ppg_info *info = pbl_ppg_get_info(ppg);
  const struct hrm_ppg_slot_config *config = agc->config;
  struct pbl_ppg_channel channel;
  int ret;

  for (uint8_t rx = 0U; rx < HRM_AGC_MAX_RX; rx++) {
    if ((config->rx_mask & (1U << rx)) == 0U) {
      continue;
    }

    ret = pbl_ppg_set_gain(ppg, config->slot, rx, info->gains_ohm[gain_idx[rx]]);
    if (ret != 0) {
      return ret;
    }

    agc->gain_idx[rx] = gain_idx[rx];
  }

  ret = pbl_ppg_set_led_current(ppg, config->slot, config->agc_led, led_ua);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_ppg_get_channel(ppg, config->slot, agc->main_rx, &channel);
  if (ret != 0) {
    return ret;
  }

  agc->led_ua = channel.led_current_ua[config->agc_led];

  return 0;
}

int hrm_agc_init(struct hrm_agc_slot *agc, const struct pbl_ppg *ppg,
                 const struct hrm_ppg_slot_config *config, uint16_t rate_hz) {
  const struct pbl_ppg_info *info = pbl_ppg_get_info(ppg);
  uint8_t gain_idx[HRM_AGC_MAX_RX];
  uint32_t led_ua = (agc->config == config) ? agc->led_ua : config->initial_ua;
  uint8_t initial_idx = prv_gain_idx(info, config->initial_gain_ohm);

  if (info == nullptr) {
    return -ENODEV;
  }

  for (uint8_t rx = 0U; rx < HRM_AGC_MAX_RX; rx++) {
    gain_idx[rx] = (agc->config == config) ? agc->gain_idx[rx] : initial_idx;
  }

  *agc = (struct hrm_agc_slot){
    .config = config,
    .rate_hz = rate_hz,
    .settle_ms = (uint32_t)pbl_uptime_ms(),
    .main_rx = (uint8_t)__builtin_ctz(config->rx_mask),
    .max_gain_idx = prv_gain_idx(info, config->max_gain_ohm),
  };

  for (uint8_t rx = 0U; rx < HRM_AGC_MAX_RX; rx++) {
    prv_window_reset(&agc->window[rx], rate_hz / SETTLE_DIV);
  }

  return prv_apply(agc, ppg, led_ua, gain_idx);
}

static void prv_scale(int64_t *v, int64_t base, int64_t num, int64_t den) {
  *v = base + ((*v - base) * num) / den;
}

static uint8_t prv_extremum(const struct hrm_agc_slot *agc, const struct pbl_ppg_info *info,
                            uint8_t gain, int64_t max, int64_t min) {
  const struct agc_levels *levels = prv_levels(agc);

  if (max > levels->high) {
    while ((max > levels->high) && (gain > 0U)) {
      prv_scale(&max, Q23_ZERO, info->gains_ohm[gain - 1U], info->gains_ohm[gain]);
      gain--;
    }
  } else if (min < levels->low) {
    while (gain < agc->max_gain_idx) {
      int64_t next = max;

      prv_scale(&next, Q23_ZERO, info->gains_ohm[gain + 1U], info->gains_ohm[gain]);
      if ((HEADROOM_DEN * next) >= (HEADROOM_NUM * levels->high)) {
        break;
      }

      max = next;
      gain++;
    }
  }

  return gain;
}

static uint32_t prv_target_ua(const struct hrm_agc_slot *agc, int64_t ipd_na) {
  const struct hrm_ppg_slot_config *config = agc->config;
  int64_t ua = ((int64_t)agc->led_ua * prv_levels(agc)->ideal_na) / ipd_na;

  return (uint32_t)CLIP(ua, (int64_t)MAX(config->min_ua, 1U), (int64_t)config->max_ua);
}

static void prv_adjust(struct hrm_agc_slot *agc, const struct pbl_ppg *ppg, int64_t mean,
                       int64_t max, int64_t min, int64_t ipd_na) {
  const struct pbl_ppg_info *info = pbl_ppg_get_info(ppg);
  const struct agc_levels *levels = prv_levels(agc);
  uint8_t gain_idx[HRM_AGC_MAX_RX];
  uint8_t gain = agc->gain_idx[agc->main_rx];
  uint32_t led_ua = agc->led_ua;
  int ret;

  for (uint8_t rx = 0U; rx < HRM_AGC_MAX_RX; rx++) {
    gain_idx[rx] = agc->gain_idx[rx];
  }

  if (agc->prev_mean_valid && (mean > levels->high) && (agc->prev_mean > levels->high)) {
    gain = 0U;
    led_ua = agc->config->min_ua;
    agc->recentered = false;
    agc->recenter_count = 0U;
  } else if ((ipd_na < levels->low_na) && agc->saturated) {
    gain = (gain > 0U) ? (gain - 1U) : 0U;
  } else {
    if ((ipd_na < levels->low_na) || (ipd_na > levels->high_na)) {
      int64_t base = MIN(Q23_ZERO, min - 1);

      led_ua = prv_target_ua(agc, ipd_na);
      prv_scale(&max, base, led_ua, agc->led_ua);
      prv_scale(&min, base, led_ua, agc->led_ua);
    }

    gain = prv_extremum(agc, info, gain, max, min);
  }

  gain_idx[agc->main_rx] = gain;

  if ((led_ua != agc->led_ua) && (agc->led_ua != 0U)) {
    for (uint8_t rx = 0U; rx < HRM_AGC_MAX_RX; rx++) {
      if ((rx == agc->main_rx) || ((agc->config->rx_mask & (1U << rx)) == 0U)) {
        continue;
      }

      gain_idx[rx] = MIN(
          prv_gain_idx(
              info, (uint32_t)(((uint64_t)info->gains_ohm[gain_idx[rx]] * agc->led_ua) / led_ua)),
          agc->max_gain_idx);
    }
  }

  ret = prv_apply(agc, ppg, led_ua, gain_idx);
  if (ret != 0) {
    PBL_LOG_ERR("AGC update of slot %u failed (%d)", agc->config->slot, ret);
  }

  PBL_LOG_DBG("AGC slot %u: %" PRIu32 " uA, gain %" PRIu32 " ohm", agc->config->slot, agc->led_ua,
              info->gains_ohm[agc->gain_idx[agc->main_rx]]);

  agc->adjusted = true;
  agc->settle_ms = (uint32_t)pbl_uptime_ms();
  agc->saturated = false;
  agc->sat_count = 0U;
  agc->sat_samples = 0U;
}

static void prv_main_window(struct hrm_agc_slot *agc, const struct pbl_ppg *ppg, int64_t mean,
                            int64_t max, int64_t min) {
  const struct pbl_ppg_info *info = pbl_ppg_get_info(ppg);
  const struct agc_levels *levels = prv_levels(agc);
  int64_t ipd_na = prv_photocurrent_na(info, info->gains_ohm[agc->gain_idx[agc->main_rx]], mean);
  bool adjust = agc->saturated || (ipd_na < levels->low_na) || (ipd_na > levels->high_na);

  if (!adjust) {
    bool off_center = ((RECENTER_DEN * mean) < (RECENTER_LOW * levels->ideal)) ||
                      ((RECENTER_DEN * mean) > (RECENTER_HIGH * levels->ideal));
    bool stable = (STABLE_MAX * max) < (STABLE_MIN * min);

    if (off_center && stable) {
      if (!agc->recentered) {
        agc->recentered = true;
        adjust = true;
      } else if (++agc->recenter_count >= RECENTER_WINDOWS) {
        adjust = true;
      }
    } else {
      agc->recenter_count = 0U;
    }
  }

  if (adjust) {
    agc->recenter_count = 0U;
    prv_adjust(agc, ppg, mean, max, min, ipd_na);
  }

  agc->prev_mean = (int32_t)mean;
  agc->prev_mean_valid = true;
}

static void prv_sub_window(struct hrm_agc_slot *agc, const struct pbl_ppg *ppg, uint8_t rx,
                           int64_t max, int64_t min) {
  const struct pbl_ppg_info *info = pbl_ppg_get_info(ppg);
  uint8_t gain = prv_extremum(agc, info, agc->gain_idx[rx], max, min);
  int ret;

  if (gain == agc->gain_idx[rx]) {
    return;
  }

  ret = pbl_ppg_set_gain(ppg, agc->config->slot, rx, info->gains_ohm[gain]);
  if (ret != 0) {
    PBL_LOG_ERR("AGC gain update of slot %u failed (%d)", agc->config->slot, ret);
    return;
  }

  agc->gain_idx[rx] = gain;
}

static int32_t prv_median3(int32_t a, int32_t b, int32_t c) {
  return MAX(MIN(a, b), MIN(MAX(a, b), c));
}

static void prv_check_saturation(struct hrm_agc_slot *agc, int64_t raw) {
  uint32_t window = SAT_WINDOW_S * agc->rate_hz;

  if (raw > prv_levels(agc)->high) {
    agc->sat_count++;
  }

  if (agc->sat_count > (window / SAT_FRACTION_DIV)) {
    agc->saturated = true;
  }

  if (++agc->sat_samples >= window) {
    agc->sat_count = 0U;
    agc->sat_samples = 0U;
  }
}

void hrm_agc_sample(struct hrm_agc_slot *agc, const struct pbl_ppg *ppg, uint8_t rx, int32_t value,
                    uint32_t time_ms) {
  const struct pbl_ppg_info *info = pbl_ppg_get_info(ppg);
  struct hrm_agc_window *w = &agc->window[rx];
  bool main = (rx == agc->main_rx);
  uint32_t length = agc->rate_hz + (main ? 0U : w->drop);
  int32_t raw = (int32_t)prv_raw(info, value);
  int32_t filtered = raw;

  if (((agc->config->rx_mask & (1U << rx)) == 0U) || ((int32_t)(time_ms - agc->settle_ms) < 0)) {
    return;
  }

  if (w->history_len == 2U) {
    filtered = prv_median3(raw, w->history[0], w->history[1]);
  }

  w->history[1] = w->history[0];
  w->history[0] = raw;
  w->history_len = MIN(w->history_len + 1U, 2U);

  if (++w->count <= w->drop) {
    return;
  }

  w->sum += filtered;
  w->max = MAX(w->max, filtered);
  w->min = MIN(w->min, filtered);

  if (main) {
    prv_check_saturation(agc, raw);
  }

  if (w->count < length) {
    return;
  }

  if (main) {
    prv_main_window(agc, ppg, w->sum / (int64_t)(w->count - w->drop), w->max, w->min);
  } else {
    prv_sub_window(agc, ppg, rx, w->max, w->min);
  }

  if (!main) {
    prv_window_reset(w, agc->rate_hz / SETTLE_DIV);
  } else if (agc->adjusted) {
    for (uint8_t i = 0U; i < HRM_AGC_MAX_RX; i++) {
      prv_window_reset(&agc->window[i], agc->rate_hz / SETTLE_DIV);
    }
  } else {
    prv_window_reset(w, 0U);
  }

  agc->adjusted = false;
}
