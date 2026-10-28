/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "hrm_agc.h"

#include <errno.h>
#include <inttypes.h>
#include <string.h>

#include <pbl/drivers/ppg.h>
#include <pbl/logging/logging.h>
#include <pbl/services/hrm/hrm_manager.h>
#include <pbl/services/hrm/hrm_ppg.h>
#include <pbl/services/system_task.h>
#include <pbl/util/math.h>
#include <pbl/util/size.h>
#include <pbl/util/units.h>

#include <kernel/pbl_malloc.h>

PBL_LOG_MODULE_DECLARE(service_hrm, CONFIG_SERVICE_HRM_LOG_LEVEL);

#define HR_RATE_HZ           25U
#define HRV_RATE_HZ          100U
#define ACCEL_RATE_HZ        (HRM_MANAGER_ACCEL_RATE_MILLIHZ / 1000U)
#define BATCH_MS_DEFAULT     3200U
#define BATCH_MS_LOW_LATENCY 1000U
#define RING_SAMPLES         512U
#define PROCESS_CHUNK        32U
#define NO_CHANNEL           UINT8_MAX

/* Physiological bounds of a wrist heart rate reading. */
#define HR_MIN_PLAUSIBLE_BPM 25U
#define HR_MAX_PLAUSIBLE_BPM 240U

struct quality_threshold {
  uint8_t min_confidence;
  HRMQuality quality;
};

static const struct quality_threshold s_hr_quality[] = {
  {98U, HRMQuality_Excellent},
  {90U, HRMQuality_Good},
  {80U, HRMQuality_Acceptable},
  {70U, HRMQuality_Poor},
};

static const struct quality_threshold s_spo2_quality[] = {
  {85U, HRMQuality_Excellent},
  {65U, HRMQuality_Good},
  {45U, HRMQuality_Acceptable},
  {25U, HRMQuality_Poor},
};

static struct {
  bool running;
  uint32_t generation;
  bool process_pending;
  struct hrm_ppg_session session;
  uint8_t channel_map[PBL_PPG_MAX_SLOTS][HRM_AGC_MAX_RX];
  struct hrm_agc_slot agc[HRM_PPG_NUM_ROLES];
  struct pbl_ppg_sample *ring;
  uint32_t *ring_ms;
  uint16_t ring_head;
  uint16_t ring_count;
  uint32_t ring_dropped;
  struct hrm_ppg_frame frame;
  uint32_t frame_filled;
  uint8_t last_channel;
  AccelRawData accel[HRM_MANAGER_MAX_ACCEL_SAMPLES];
  uint32_t accel_count;
  uint32_t accel_next;
  uint32_t accel_phase;
  bool is_wear;
  bool spo2_assumed_wear;
  bool wear_before_spo2;
} s_ppg;

static HRMQuality prv_quality(const struct quality_threshold *thresholds, size_t num,
                              uint8_t confidence) {
  for (size_t i = 0U; i < num; i++) {
    if (confidence >= thresholds[i].min_confidence) {
      return thresholds[i].quality;
    }
  }

  return HRMQuality_Worst;
}

void hrm_ppg_report_bpm(uint8_t bpm, uint8_t confidence) {
  HRMData data = {.features = HRMFeature_BPM};

  PBL_LOG_DBG("BPM %" PRIu8 " (confidence %" PRIu8 ", wear %u)", bpm, confidence, s_ppg.is_wear);

  if (!s_ppg.is_wear) {
    data.hrm_quality = HRMQuality_OffWrist;
  } else if ((bpm < HR_MIN_PLAUSIBLE_BPM) || (bpm > HR_MAX_PLAUSIBLE_BPM)) {
    PBL_LOG_WRN("Dropping implausible BPM %" PRIu8, bpm);
    data.hrm_quality = HRMQuality_Worst;
  } else {
    data.hrm_bpm = bpm;
    data.hrm_quality = prv_quality(s_hr_quality, ARRAY_LENGTH(s_hr_quality), confidence);
  }

  hrm_manager_new_data_cb(&data);
}

void hrm_ppg_report_spo2(uint8_t percent, uint8_t confidence, uint8_t valid_level, bool invalid) {
  HRMData data = {
    .features = HRMFeature_SpO2,
    .spo2_percent = percent,
    .spo2_confidence = confidence,
    .spo2_valid_level = valid_level,
    .spo2_invalid = invalid,
  };

  PBL_LOG_DBG("SpO2 %" PRIu8 " (confidence %" PRIu8 ", valid %" PRIu8 ", invalid %u, wear %u)",
              percent, confidence, valid_level, invalid, s_ppg.is_wear);

  if (!s_ppg.is_wear) {
    data.spo2_quality = HRMQuality_OffWrist;
  } else if (invalid) {
    data.spo2_quality = HRMQuality_Worst;
  } else {
    data.spo2_quality = prv_quality(s_spo2_quality, ARRAY_LENGTH(s_spo2_quality), confidence);
  }

  hrm_manager_new_data_cb(&data);
}

void hrm_ppg_report_hrv(const int32_t *rri_ms, uint8_t num, uint8_t confidence) {
  PBL_LOG_DBG("HRV n=%u (confidence %" PRIu8 ", wear %u)", num, confidence, s_ppg.is_wear);

  if (!s_ppg.is_wear) {
    const HRMData data = {.features = HRMFeature_HRV, .hrv_quality = HRMQuality_OffWrist};

    hrm_manager_new_data_cb(&data);
    return;
  }

  for (uint8_t i = 0U; i < num; i++) {
    HRMData data = {.features = HRMFeature_HRV};

    if ((rri_ms[i] <= 0) || (rri_ms[i] > UINT16_MAX)) {
      continue;
    }

    data.hrv_ppi_ms = (uint16_t)rri_ms[i];
    data.hrv_quality = prv_quality(s_hr_quality, ARRAY_LENGTH(s_hr_quality), confidence);
    hrm_manager_new_data_cb(&data);
  }
}

void hrm_ppg_report_wear(bool worn) {
  PBL_LOG_DBG("Wear %u", worn);

  s_ppg.is_wear = worn;
}

static void prv_set_wear_policy(bool spo2_path) {
  if (spo2_path) {
    if (!s_ppg.spo2_assumed_wear) {
      s_ppg.wear_before_spo2 = s_ppg.is_wear;
      s_ppg.spo2_assumed_wear = true;
    }

    s_ppg.is_wear = true;
  } else if (s_ppg.spo2_assumed_wear) {
    s_ppg.is_wear = s_ppg.wear_before_spo2;
    s_ppg.spo2_assumed_wear = false;
  }
}

static void prv_take_accel(void) {
  HRMAccelData *accel = hrm_manager_get_accel_data();
  uint32_t n = MIN(accel->num_samples, ARRAY_LENGTH(s_ppg.accel));

  if (n > 0U) {
    memcpy(s_ppg.accel, accel->data, n * sizeof(s_ppg.accel[0]));
    s_ppg.accel_count = n;
    s_ppg.accel_next = 0U;
  }

  hrm_manager_release_accel_data();
}

static void prv_frame_accel(struct hrm_ppg_frame *frame) {
  const AccelRawData *sample;

  if (s_ppg.accel_count == 0U) {
    return;
  }

  sample = &s_ppg.accel[s_ppg.accel_next];
  frame->accel_mg[0] = sample->x;
  frame->accel_mg[1] = sample->y;
  frame->accel_mg[2] = sample->z;

  s_ppg.accel_phase += ACCEL_RATE_HZ;
  while ((s_ppg.accel_phase >= s_ppg.session.sample_rate_hz) &&
         ((s_ppg.accel_next + 1U) < s_ppg.accel_count)) {
    s_ppg.accel_phase -= s_ppg.session.sample_rate_hz;
    s_ppg.accel_next++;
  }

  s_ppg.accel_phase = MIN(s_ppg.accel_phase, s_ppg.session.sample_rate_hz);
}

static void prv_handle_sample(const struct pbl_ppg_sample *sample, uint32_t time_ms) {
  const struct hrm_ppg_session *session = &s_ppg.session;
  uint8_t idx;
  struct hrm_ppg_channel channel;
  struct hrm_agc_slot *agc;

  if ((sample->slot >= PBL_PPG_MAX_SLOTS) || (sample->rx >= HRM_AGC_MAX_RX)) {
    return;
  }

  idx = s_ppg.channel_map[sample->slot][sample->rx];
  if (idx == NO_CHANNEL) {
    return;
  }

  channel = session->channels[idx];
  agc = &s_ppg.agc[channel.role];

  if ((idx <= s_ppg.last_channel) && (s_ppg.last_channel != NO_CHANNEL)) {
    s_ppg.frame_filled = 0U;
  }

  s_ppg.last_channel = idx;
  s_ppg.frame.value[idx] = sample->value;
  s_ppg.frame.led_ua[idx] = agc->led_ua;
  s_ppg.frame.gain_ohm[idx] = session->info->gains_ohm[agc->gain_idx[sample->rx]];
  s_ppg.frame_filled |= 1U << idx;

  if (HRM_PPG_CONFIG.roles[channel.role].agc) {
    hrm_agc_sample(agc, PPG, sample->rx, sample->value, time_ms);
  }

  if (s_ppg.frame_filled == ((1U << session->num_channels) - 1U)) {
    prv_frame_accel(&s_ppg.frame);
    HRM_PPG_BACKEND.process(&s_ppg.frame);
    s_ppg.frame_filled = 0U;
  }
}

static uint16_t prv_pop(struct pbl_ppg_sample *out, uint32_t *out_ms, uint32_t generation) {
  uint16_t n = 0U;

  pbl_ppg_lock(PPG);

  if (s_ppg.running && (s_ppg.generation == generation)) {
    while ((n < PROCESS_CHUNK) && (s_ppg.ring_count > 0U)) {
      uint16_t tail = (s_ppg.ring_head + RING_SAMPLES - s_ppg.ring_count) % RING_SAMPLES;

      out_ms[n] = s_ppg.ring_ms[tail];
      out[n++] = s_ppg.ring[tail];
      s_ppg.ring_count--;
    }
  }

  if (n == 0U) {
    s_ppg.process_pending = false;
  }

  pbl_ppg_unlock(PPG);

  return n;
}

static void prv_process(void *data) {
  uint32_t generation = (uint32_t)(uintptr_t)data;
  struct pbl_ppg_sample samples[PROCESS_CHUNK];
  uint32_t times_ms[PROCESS_CHUNK];
  uint16_t n;

  prv_take_accel();

  while ((n = prv_pop(samples, times_ms, generation)) > 0U) {
    for (uint16_t i = 0U; i < n; i++) {
      prv_handle_sample(&samples[i], times_ms[i]);
    }
  }
}

static void prv_samples(const struct pbl_ppg *ppg, const struct pbl_ppg_batch *batch,
                        void *user_data) {
  uint32_t period_samples = MAX(s_ppg.session.num_channels, 1U);

  if (!s_ppg.running) {
    return;
  }

  for (uint16_t i = 0U; i < batch->num_samples; i++) {
    uint32_t later_periods = (batch->num_samples - 1U - i) / period_samples;

    if (s_ppg.ring_count == RING_SAMPLES) {
      s_ppg.ring_dropped++;
      continue;
    }

    s_ppg.ring_ms[s_ppg.ring_head] =
        (uint32_t)(batch->timestamp_ms -
                   (later_periods * PBL_MSEC_PER_SEC) / batch->sample_rate_hz);
    s_ppg.ring[s_ppg.ring_head] = batch->samples[i];
    s_ppg.ring_head = (s_ppg.ring_head + 1U) % RING_SAMPLES;
    s_ppg.ring_count++;
  }

  if (s_ppg.ring_dropped > 0U) {
    PBL_LOG_WRN("Dropped %" PRIu32 " samples", s_ppg.ring_dropped);
    s_ppg.ring_dropped = 0U;
  }

  if (!s_ppg.process_pending) {
    s_ppg.process_pending =
        system_task_add_callback(prv_process, (void *)(uintptr_t)s_ppg.generation);
  }
}

static const struct pbl_ppg_listener s_listener = {.samples = prv_samples};

static uint32_t prv_roles(HRMFeature features) {
  uint32_t roles = 0U;
  bool spo2 = (features & HRMFeature_SpO2) != 0U;
  bool green = (features & (HRMFeature_BPM | HRMFeature_HRV)) != 0U;

#ifndef CONFIG_MFG
  if (spo2) {
    green = false;
  }
#endif

  if (spo2) {
    roles |= (1U << HRM_PPG_ROLE_RED) | (1U << HRM_PPG_ROLE_IR);
  }

  if (green || !spo2) {
    roles |= (1U << HRM_PPG_ROLE_GREEN) | (1U << HRM_PPG_ROLE_WEAR) | (1U << HRM_PPG_ROLE_AMBIENT);
  }

  return roles;
}

static int prv_build_session(HRMFeature features, struct pbl_ppg_config *config) {
  struct hrm_ppg_session *session = &s_ppg.session;
  uint32_t roles = prv_roles(features);
  uint8_t slot_role[PBL_PPG_MAX_SLOTS];

  memset(s_ppg.channel_map, NO_CHANNEL, sizeof(s_ppg.channel_map));
  session->num_channels = 0U;

  for (uint8_t role = 0U; role < HRM_PPG_NUM_ROLES; role++) {
    const struct hrm_ppg_slot_config *slot = &HRM_PPG_CONFIG.roles[role];

    if (((roles & (1U << role)) == 0U) || (slot->rx_mask == 0U)) {
      continue;
    }

    if ((slot->slot >= PBL_PPG_MAX_SLOTS) || (config->rx_mask[slot->slot] != 0U)) {
      return -EINVAL;
    }

    config->rx_mask[slot->slot] = slot->rx_mask;
    slot_role[slot->slot] = role;
  }

  for (uint8_t slot = 0U; slot < PBL_PPG_MAX_SLOTS; slot++) {
    for (uint8_t rx = 0U; rx < HRM_AGC_MAX_RX; rx++) {
      if ((config->rx_mask[slot] & (1U << rx)) == 0U) {
        continue;
      }

      if (session->num_channels == HRM_PPG_MAX_CHANNELS) {
        return -EINVAL;
      }

      s_ppg.channel_map[slot][rx] = session->num_channels;
      session->channels[session->num_channels++] = (struct hrm_ppg_channel){
        .role = (enum hrm_ppg_role)slot_role[slot],
        .rx = rx,
      };
    }
  }

  return (session->num_channels > 0U) ? 0 : -EINVAL;
}

static int prv_read_settings(struct hrm_agc_slot *agc, const struct hrm_ppg_slot_config *slot) {
  const struct pbl_ppg_info *info = s_ppg.session.info;
  const struct pbl_ppg_slot *hw = &info->slots[slot->slot];
  struct pbl_ppg_channel channel;
  int ret;

  agc->config = slot;
  agc->led_ua = 0U;

  for (uint8_t rx = 0U; rx < HRM_AGC_MAX_RX; rx++) {
    if ((slot->rx_mask & (1U << rx)) == 0U) {
      continue;
    }

    ret = pbl_ppg_get_channel(PPG, slot->slot, rx, &channel);
    if (ret != 0) {
      return ret;
    }

    agc->gain_idx[rx] = 0U;
    while (((agc->gain_idx[rx] + 1U) < info->num_gains) &&
           (info->gains_ohm[agc->gain_idx[rx]] < channel.gain_ohm)) {
      agc->gain_idx[rx]++;
    }

    agc->led_ua = 0U;
    for (uint8_t led = 0U; led < PBL_PPG_MAX_LED_DRIVERS; led++) {
      if (hw->led_mask & (1U << led)) {
        agc->led_ua += channel.led_current_ua[led];
      }
    }
  }

  return 0;
}

static int prv_start_channels(void) {
  int ret;

  for (uint8_t role = 0U; role < HRM_PPG_NUM_ROLES; role++) {
    const struct hrm_ppg_slot_config *slot = &HRM_PPG_CONFIG.roles[role];
    bool used = false;

    for (uint8_t i = 0U; i < s_ppg.session.num_channels; i++) {
      used |= (s_ppg.session.channels[i].role == role);
    }

    if (!used) {
      continue;
    }

    if (slot->agc) {
      ret = hrm_agc_init(&s_ppg.agc[role], PPG, slot, s_ppg.session.sample_rate_hz);
    } else {
      ret = prv_read_settings(&s_ppg.agc[role], slot);
    }

    if (ret != 0) {
      return ret;
    }
  }

  return 0;
}

static int prv_start_locked(HRMFeature features, bool low_latency) {
  struct hrm_ppg_session *session = &s_ppg.session;
  struct pbl_ppg_config config = {};
  uint32_t batch_ms = low_latency ? BATCH_MS_LOW_LATENCY : BATCH_MS_DEFAULT;
  int ret;

  session->info = pbl_ppg_get_info(PPG);
  if (session->info == nullptr) {
    return -ENODEV;
  }

  session->features = features;
#ifdef CONFIG_HRM_HRV
  session->sample_rate_hz = (features & HRMFeature_HRV) ? HRV_RATE_HZ : HR_RATE_HZ;
#else
  session->sample_rate_hz = HR_RATE_HZ;
#endif

  ret = prv_build_session(features, &config);
  if (ret != 0) {
    return ret;
  }

  config.sample_rate_hz = session->sample_rate_hz;
  config.batch = (uint16_t)MAX((batch_ms * session->sample_rate_hz) / PBL_MSEC_PER_SEC, 1U);

  ret = pbl_ppg_configure(PPG, &config);
  if (ret != 0) {
    return ret;
  }

  ret = prv_start_channels();
  if (ret != 0) {
    return ret;
  }

  ret = HRM_PPG_BACKEND.start(session);
  if (ret != 0) {
    return ret;
  }

  s_ppg.ring_head = 0U;
  s_ppg.ring_count = 0U;
  s_ppg.frame_filled = 0U;
  s_ppg.last_channel = NO_CHANNEL;
  s_ppg.accel_count = 0U;
  s_ppg.accel_phase = 0U;
  s_ppg.generation++;
  s_ppg.process_pending = false;
  s_ppg.running = true;

  ret = pbl_ppg_start(PPG);
  if (ret != 0) {
    s_ppg.running = false;
    HRM_PPG_BACKEND.stop();
    return ret;
  }

  return 0;
}

static void prv_free_ring(void) {
  kernel_free(s_ppg.ring);
  kernel_free(s_ppg.ring_ms);
  s_ppg.ring = nullptr;
  s_ppg.ring_ms = nullptr;
}

int hrm_ppg_start(HRMFeature features, bool low_latency) {
  int ret;

  if (s_ppg.running) {
    hrm_ppg_stop();
  }

  s_ppg.ring = kernel_malloc(RING_SAMPLES * sizeof(*s_ppg.ring));
  s_ppg.ring_ms = kernel_malloc(RING_SAMPLES * sizeof(*s_ppg.ring_ms));
  if ((s_ppg.ring == nullptr) || (s_ppg.ring_ms == nullptr)) {
    prv_free_ring();
    return -ENOMEM;
  }

  prv_set_wear_policy((prv_roles(features) & (1U << HRM_PPG_ROLE_GREEN)) == 0U);

  pbl_ppg_lock(PPG);
  (void)pbl_ppg_set_listener(PPG, &s_listener);
  ret = prv_start_locked(features, low_latency);
  if (ret != 0) {
    (void)pbl_ppg_set_listener(PPG, nullptr);
  }
  pbl_ppg_unlock(PPG);

  if (ret != 0) {
    prv_free_ring();
    PBL_LOG_ERR("Could not start the front end (%d)", ret);
  }

  return ret;
}

void hrm_ppg_stop(void) {
  int ret;

  if (!s_ppg.running) {
    return;
  }

  pbl_ppg_lock(PPG);
  s_ppg.running = false;
  ret = pbl_ppg_stop(PPG);
  (void)pbl_ppg_set_listener(PPG, nullptr);
  prv_free_ring();
  s_ppg.ring_count = 0U;
  pbl_ppg_unlock(PPG);

  if (ret != 0) {
    PBL_LOG_ERR("Could not stop the front end (%d)", ret);
  }

  HRM_PPG_BACKEND.stop();
}

bool hrm_ppg_is_running(void) {
  return s_ppg.running;
}

void hrm_ppg_set_scene(HRMActivityScene scene) {
  if (HRM_PPG_BACKEND.set_scene != nullptr) {
    HRM_PPG_BACKEND.set_scene(scene);
  }
}
