/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/ppg/qemu.h>
#include <pbl/kernel/time.h>
#include <pbl/services/new_timer/new_timer.h>
#include <pbl/util/math.h>
#include <pbl/util/size.h>
#include <pbl/util/trig.h>
#include <pbl/util/units.h>

#define QEMU_PPG_NUM_SLOTS         4U
#define QEMU_PPG_MAX_RATE_HZ       1000U
#define QEMU_PPG_MAX_CURRENT_UA    200000U
#define QEMU_PPG_FULL_SCALE        0x800000
#define QEMU_PPG_FULL_SCALE_UV     900000U
#define QEMU_PPG_CHUNK             32U
#define QEMU_PPG_DEFAULT_UA        20000U
#define QEMU_PPG_DEFAULT_OHM       100000U
#define QEMU_PPG_AMBIENT_PA        50000LL
#define QEMU_PPG_LEAKAGE_PA_PER_UA 1LL
#define QEMU_PPG_SEC_PER_MIN       60U
#define QEMU_PPG_PERMILLE          1000
#define QEMU_PPG_RED_IR_RATIO_PCT  50
#define QEMU_PPG_PERCENT           100

enum {
  QEMU_PPG_SLOT_GREEN,
  QEMU_PPG_SLOT_RED,
  QEMU_PPG_SLOT_IR,
  QEMU_PPG_SLOT_AMBIENT,
};

static const struct pbl_ppg_slot s_slots[QEMU_PPG_NUM_SLOTS] = {
  [QEMU_PPG_SLOT_GREEN] = {.led_mask = 0x1, .leds = {PBL_PPG_LED_GREEN}, .rx_mask = 0x1},
  [QEMU_PPG_SLOT_RED] = {.led_mask = 0x2, .leds = {[1] = PBL_PPG_LED_RED}, .rx_mask = 0x1},
  [QEMU_PPG_SLOT_IR] = {.led_mask = 0x1, .leds = {PBL_PPG_LED_IR}, .rx_mask = 0x1},
  [QEMU_PPG_SLOT_AMBIENT] = {.rx_mask = 0x1},
};

/* Light coming back from a wrist per LED current, in pA per uA. */
static const int64_t s_ctr[QEMU_PPG_NUM_SLOTS] = {
  [QEMU_PPG_SLOT_GREEN] = 100,
  [QEMU_PPG_SLOT_RED] = 1000,
  [QEMU_PPG_SLOT_IR] = 1000,
};

static const uint32_t s_gains_ohm[] = {
  10000U,  25000U,   50000U,   75000U,   100000U,  250000U,  500000U,
  750000U, 1000000U, 1250000U, 1500000U, 1750000U, 2000000U,
};

static const struct pbl_ppg_info s_info = {
  .slots = s_slots,
  .num_slots = QEMU_PPG_NUM_SLOTS,
  .num_rx = 1U,
  .max_led_current_ua = QEMU_PPG_MAX_CURRENT_UA,
  .gains_ohm = s_gains_ohm,
  .num_gains = ARRAY_LENGTH(s_gains_ohm),
  .full_scale = QEMU_PPG_FULL_SCALE,
  .full_scale_uv = QEMU_PPG_FULL_SCALE_UV,
  .max_sample_rate_hz = QEMU_PPG_MAX_RATE_HZ,
  .fifo_depth = QEMU_PPG_CHUNK,
};

static struct {
  TimerID timer;
  struct pbl_ppg_config config;
  uint32_t led_ua[QEMU_PPG_NUM_SLOTS][PBL_PPG_MAX_LED_DRIVERS];
  uint32_t gain_ohm[QEMU_PPG_NUM_SLOTS];
  uint16_t bpm;
  uint16_t perfusion_permille;
  bool pulse_set;
  uint32_t phase;
  struct pbl_ppg_sample samples[QEMU_PPG_CHUNK];
} s_qemu;

static int32_t prv_value(uint8_t slot, int32_t sine) {
  int64_t pa = QEMU_PPG_AMBIENT_PA;
  int64_t led_ua = (int64_t)s_qemu.led_ua[slot][0] + s_qemu.led_ua[slot][1];
  int64_t value;

  if (s_qemu.perfusion_permille == 0U) {
    pa += led_ua * QEMU_PPG_LEAKAGE_PA_PER_UA;
  } else {
    int64_t modulation = s_qemu.perfusion_permille;
    int64_t tissue = led_ua * s_ctr[slot];

    if (slot == QEMU_PPG_SLOT_RED) {
      modulation = (modulation * QEMU_PPG_RED_IR_RATIO_PCT) / QEMU_PPG_PERCENT;
    }

    pa += tissue - (tissue * modulation * (TRIG_MAX_RATIO + sine)) /
                       (2LL * QEMU_PPG_PERMILLE * TRIG_MAX_RATIO);
  }

  value = (pa * s_qemu.gain_ohm[slot]) / PBL_USEC_PER_SEC;
  value = (value * QEMU_PPG_FULL_SCALE) / QEMU_PPG_FULL_SCALE_UV;

  return (int32_t)CLIP(value, -QEMU_PPG_FULL_SCALE, QEMU_PPG_FULL_SCALE - 1);
}

static void prv_report(const struct pbl_ppg *ppg, uint16_t num_samples, uint64_t timestamp_ms) {
  const struct pbl_ppg_batch batch = {
    .samples = s_qemu.samples,
    .num_samples = num_samples,
    .sample_rate_hz = s_qemu.config.sample_rate_hz,
    .timestamp_ms = timestamp_ms,
  };

  pbl_ppg_report_samples(ppg, &batch);
}

static void prv_timer_cb(void *data) {
  const struct pbl_ppg *ppg = data;
  const struct pbl_ppg_config *config = &s_qemu.config;
  uint32_t step = (TRIG_MAX_ANGLE * (uint32_t)s_qemu.bpm) /
                  (QEMU_PPG_SEC_PER_MIN * (uint32_t)config->sample_rate_hz);
  uint64_t now_ms = pbl_uptime_ms();
  uint64_t period_ms = 0U;
  uint16_t n = 0U;

  pbl_ppg_lock(ppg);

  if (!ppg->state->running || !s_qemu.pulse_set) {
    pbl_ppg_unlock(ppg);
    return;
  }

  for (uint16_t period = 0U; period < config->batch; period++) {
    int32_t sine = sin_lookup((int32_t)(s_qemu.phase % TRIG_MAX_ANGLE));

    s_qemu.phase += step;
    period_ms = now_ms - ((uint64_t)(config->batch - 1U - period) * PBL_MSEC_PER_SEC) /
                             config->sample_rate_hz;

    for (uint8_t slot = 0U; slot < QEMU_PPG_NUM_SLOTS; slot++) {
      if (config->rx_mask[slot] == 0U) {
        continue;
      }

      s_qemu.samples[n++] = (struct pbl_ppg_sample){
        .value = prv_value(slot, sine),
        .slot = slot,
      };

      if (n == QEMU_PPG_CHUNK) {
        prv_report(ppg, n, period_ms);
        n = 0U;
      }
    }
  }

  if (n > 0U) {
    prv_report(ppg, n, period_ms);
  }

  pbl_ppg_unlock(ppg);
}

static int prv_init(const struct pbl_ppg *ppg) {
  s_qemu.timer = new_timer_create();
  if (s_qemu.timer == TIMER_INVALID_ID) {
    return -ENOMEM;
  }

  for (uint8_t slot = 0U; slot < QEMU_PPG_NUM_SLOTS; slot++) {
    for (uint8_t led = 0U; led < PBL_PPG_MAX_LED_DRIVERS; led++) {
      if (s_slots[slot].led_mask & (1U << led)) {
        s_qemu.led_ua[slot][led] = QEMU_PPG_DEFAULT_UA;
      }
    }

    s_qemu.gain_ohm[slot] = QEMU_PPG_DEFAULT_OHM;
  }

  return 0;
}

static const struct pbl_ppg_info *prv_get_info(const struct pbl_ppg *ppg) {
  return &s_info;
}

static int prv_configure(const struct pbl_ppg *ppg, const struct pbl_ppg_config *config) {
  s_qemu.config = *config;

  return 0;
}

static int prv_start(const struct pbl_ppg *ppg) {
  const struct pbl_ppg_config *config = &s_qemu.config;
  uint32_t period_ms =
      MAX(((uint32_t)config->batch * PBL_MSEC_PER_SEC) / config->sample_rate_hz, 1U);

  if (!new_timer_start(s_qemu.timer, period_ms, prv_timer_cb, (void *)ppg,
                       TIMER_START_FLAG_REPEATING)) {
    return -EIO;
  }

  return 0;
}

static int prv_stop(const struct pbl_ppg *ppg) {
  (void)new_timer_stop(s_qemu.timer);

  return 0;
}

static int prv_set_led_current(const struct pbl_ppg *ppg, uint8_t slot, uint8_t led, uint32_t ua) {
  s_qemu.led_ua[slot][led] = ua;

  return 0;
}

static int prv_set_gain(const struct pbl_ppg *ppg, uint8_t slot, uint8_t rx, uint32_t ohm) {
  s_qemu.gain_ohm[slot] = ohm;

  return 0;
}

static int prv_get_channel(const struct pbl_ppg *ppg, uint8_t slot, uint8_t rx,
                           struct pbl_ppg_channel *channel) {
  for (uint8_t led = 0U; led < PBL_PPG_MAX_LED_DRIVERS; led++) {
    channel->led_current_ua[led] = s_qemu.led_ua[slot][led];
    channel->calibrated_led_current_ua[led] = s_qemu.led_ua[slot][led];
  }

  channel->gain_ohm = s_qemu.gain_ohm[slot];
  channel->calibrated_gain_ohm = s_qemu.gain_ohm[slot];

  return 0;
}

static const struct pbl_ppg_ops s_ops = {
  .init = prv_init,
  .get_info = prv_get_info,
  .configure = prv_configure,
  .start = prv_start,
  .stop = prv_stop,
  .set_led_current = prv_set_led_current,
  .set_gain = prv_set_gain,
  .get_channel = prv_get_channel,
};

PBL_PPG_STATE_DEFINE(pbl_ppg_qemu);
const struct pbl_ppg pbl_ppg_qemu = PBL_PPG_INIT(pbl_ppg_qemu, "ppg", nullptr, nullptr, &s_ops);
PBL_DEVICE_REGISTER(pbl_ppg_qemu, &pbl_ppg_qemu.dev);

void qemu_ppg_set_pulse(uint16_t bpm, uint16_t perfusion_permille) {
  pbl_ppg_lock(QEMU_PPG);
  s_qemu.bpm = bpm;
  s_qemu.perfusion_permille = MIN(perfusion_permille, QEMU_PPG_PERMILLE);
  s_qemu.pulse_set = true;
  pbl_ppg_unlock(QEMU_PPG);
}
