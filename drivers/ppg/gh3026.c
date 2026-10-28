/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "gh3026_regs.h"

#include <errno.h>
#include <inttypes.h>

#include <pbl/drivers/ppg/gh3026.h>
#include <pbl/kernel/delay.h>
#include <pbl/kernel/time.h>
#include <pbl/logging/logging.h>
#include <pbl/services/new_timer/new_timer.h>
#include <pbl/util/math.h>
#include <pbl/util/misc.h>
#include <pbl/util/size.h>
#include <pbl/util/units.h>

PBL_LOG_MODULE_DEFINE(driver_ppg_gh3026, CONFIG_DRIVER_PPG_LOG_LEVEL);

#define GH3026_NUM_RX            2U
#define GH3026_NUM_LED_DRIVERS   2U
#define GH3026_OSC_HZ            32000U
#define GH3026_SR_MULT_MAX       256U
#define GH3026_FIFO_WORDS        800U
#define GH3026_WATERMARK_MIN     3U
#define GH3026_RESET_ASSERT_MS   1U
#define GH3026_RESET_DELAY_MS    30U
#define GH3026_WAKEUP_US         500U
#define GH3026_EFUSE_POLL_MS     1U
#define GH3026_EFUSE_TIMEOUT_MS  200U
#define GH3026_DRV_CODE_MAX      255U
#define GH3026_FULL_SCALE        0x800000
#define GH3026_FULL_SCALE_UV     900000U
#define GH3026_SATURATION_MARGIN 0x8000
#define GH3026_LED_FS_MAX_CODE   3U

/* Efuse LED calibration: I = V / (Er * 255 * N / code + Y), Er = 1 + ecode / 256. */
#define GH3026_CAL_V_UV      225000LL
#define GH3026_CAL_ECODE_DEN 256
#define GH3026_CAL_SCALE     1000000LL

static const uint32_t s_gains_ohm[] = {
  10000U,  25000U,   50000U,   75000U,   100000U,  250000U,  500000U,
  750000U, 1000000U, 1250000U, 1500000U, 1750000U, 2000000U,
};

static const uint32_t s_led_fs_ua[] = {25000U, 50000U, 100000U, 200000U};

static const int64_t s_cal_y_micro[GH3026_NUM_LED_DRIVERS] = {100000LL, 10000LL};

static const struct pbl_gh3026 *prv_dev(const struct pbl_ppg *ppg) {
  return container_of(ppg, const struct pbl_gh3026, ppg);
}

static int prv_write(const struct pbl_gh3026 *gh, uint16_t addr, uint16_t val) {
  const uint8_t buf[] = {addr >> 8, addr & 0xFFU, val >> 8, val & 0xFFU};

  return pbl_i2c_write(&gh->i2c, buf, sizeof(buf));
}

static int prv_read(const struct pbl_gh3026 *gh, uint16_t addr, uint16_t *val) {
  const uint8_t wbuf[] = {addr >> 8, addr & 0xFFU};
  uint8_t rbuf[2];
  int ret;

  ret = pbl_i2c_write_read(&gh->i2c, wbuf, sizeof(wbuf), rbuf, sizeof(rbuf));
  if (ret != 0) {
    return ret;
  }

  *val = ((uint16_t)rbuf[0] << 8) | rbuf[1];

  return 0;
}

static int prv_update(const struct pbl_gh3026 *gh, uint16_t addr, uint16_t mask, uint16_t val) {
  uint16_t reg;
  int ret;

  ret = prv_read(gh, addr, &reg);
  if (ret != 0) {
    return ret;
  }

  return prv_write(gh, addr, (reg & ~mask) | (val & mask));
}

static int prv_cmd(const struct pbl_gh3026 *gh, uint8_t cmd) {
  const uint8_t buf[] = {GH3026_CMD_ADDR >> 8, GH3026_CMD_ADDR & 0xFFU, cmd};

  return pbl_i2c_write(&gh->i2c, buf, sizeof(buf));
}

static int prv_wakeup(const struct pbl_gh3026 *gh) {
  int ret;

  ret = prv_cmd(gh, GH3026_CMD_WAKEUP);
  if (ret != 0) {
    return ret;
  }

  pbl_sleep_us(GH3026_WAKEUP_US);

  return 0;
}

static int prv_finish(const struct pbl_gh3026 *gh, int ret) {
  int sleep_ret = prv_cmd(gh, GH3026_CMD_SLEEP);

  return (ret != 0) ? ret : sleep_ret;
}

static int prv_reset(const struct pbl_gh3026 *gh) {
  int ret;

  if (!pbl_gpio_is_connected(&gh->config.reset)) {
    ret = prv_cmd(gh, GH3026_CMD_RESET);
    if (ret != 0) {
      return ret;
    }

    pbl_sleep_ms(GH3026_RESET_DELAY_MS);
    return 0;
  }

  ret = pbl_gpio_set(&gh->config.reset, true);
  if (ret != 0) {
    return ret;
  }

  pbl_sleep_ms(GH3026_RESET_ASSERT_MS);

  ret = pbl_gpio_set(&gh->config.reset, false);
  if (ret != 0) {
    return ret;
  }

  pbl_sleep_ms(GH3026_RESET_DELAY_MS);

  return 0;
}

static int prv_probe(const struct pbl_gh3026 *gh) {
  uint16_t ready;
  uint16_t chip_id;
  uint16_t product[2];
  int ret;

  ret = prv_wakeup(gh);
  if (ret != 0) {
    return -ENODEV;
  }

  ret = prv_read(gh, GH3026_READY, &ready);
  if ((ret != 0) || (ready != GH3026_READY_CODE)) {
    return -ENODEV;
  }

  ret = prv_read(gh, GH3026_CHIP_ID, &chip_id);
  if (ret != 0) {
    return ret;
  }

  ret = prv_read(gh, GH3026_PRODUCT_ID_L, &product[0]);
  if (ret != 0) {
    return ret;
  }

  ret = prv_read(gh, GH3026_PRODUCT_ID_H, &product[1]);
  if (ret != 0) {
    return ret;
  }

  PBL_LOG_DBG("chip id 0x%04" PRIx16 ", product 0x%04" PRIx16 " 0x%04" PRIx16, chip_id, product[0],
              product[1]);

  return 0;
}

static int prv_read_efuse(const struct pbl_gh3026 *gh) {
  uint16_t status;
  uint16_t rdata;
  int ret;

  ret = prv_write(gh, GH3026_EFUSE_CTRL, GH3026_EFUSE_CTRL_ENABLE);
  if (ret != 0) {
    return ret;
  }

  ret = prv_write(gh, GH3026_EFUSE_CTRL, GH3026_EFUSE_CTRL_READ0);
  if (ret != 0) {
    return ret;
  }

  ret = PBL_POLL_SLEEP_MS(
      (prv_read(gh, GH3026_EFUSE_STATUS, &status) == 0) && (status & GH3026_EFUSE_STATUS_DONE),
      GH3026_EFUSE_POLL_MS, GH3026_EFUSE_TIMEOUT_MS);
  if (ret != 0) {
    return ret;
  }

  ret = prv_read(gh, GH3026_EFUSE_RDATA1, &rdata);
  if (ret != 0) {
    return ret;
  }

  gh->state->led_ecode = (int8_t)PBL_FIELD_GET(GH3026_EFUSE_RDATA_LED, rdata);

  return prv_write(gh, GH3026_EFUSE_CTRL, 0U);
}

static int prv_read_setup(const struct pbl_gh3026 *gh) {
  struct pbl_gh3026_state *state = gh->state;
  uint16_t reg;
  int ret;

  ret = prv_read(gh, GH3026_SAMPLE_RATE, &reg);
  if (ret != 0) {
    return ret;
  }

  state->base_rate_hz = GH3026_OSC_HZ / (PBL_FIELD_GET(GH3026_SAMPLE_RATE_DIV, reg) + 1U);

  for (uint8_t pos = 0U; pos < gh->config.num_slots; pos++) {
    ret = prv_read(gh, GH3026_SLOT_TIMER(pos), &state->slot_timers[pos]);
    if (ret != 0) {
      return ret;
    }
  }

  ret = prv_read(gh, GH3026_LED_FS, &reg);
  if (ret != 0) {
    return ret;
  }

  for (uint8_t led = 0U; led < GH3026_NUM_LED_DRIVERS; led++) {
    state->led_fs_ua[led] = s_led_fs_ua[PBL_FIELD_GET(GH3026_LED_FS_DRV(led), reg)];
  }

  return 0;
}

static int prv_init_awake(const struct pbl_gh3026 *gh) {
  const struct pbl_gh3026_config *config = &gh->config;
  int ret;

  ret = prv_probe(gh);
  if (ret != 0) {
    return ret;
  }

  ret = prv_write(gh, GH3026_INT_STATUS, GH3026_INT_ALL);
  if (ret != 0) {
    return ret;
  }

  for (uint16_t i = 0U; i < config->num_regs; i++) {
    ret = prv_write(gh, config->regs[i].addr, config->regs[i].val);
    if (ret != 0) {
      return ret;
    }
  }

  ret = prv_read_setup(gh);
  if (ret != 0) {
    return ret;
  }

  return prv_read_efuse(gh);
}

static void prv_drain(const struct pbl_gh3026 *gh);

static void prv_irq_work(void *data) {
  const struct pbl_gh3026 *gh = data;
  uint16_t status;
  int ret;

  pbl_ppg_lock(&gh->ppg);

  ret = prv_wakeup(gh);
  if (ret != 0) {
    PBL_LOG_ERR("Wakeup failed (%d)", ret);
    pbl_ppg_unlock(&gh->ppg);
    return;
  }

  ret = prv_read(gh, GH3026_INT_STATUS, &status);
  if (ret == 0) {
    ret = prv_write(gh, GH3026_INT_STATUS, status);
  }

  if (ret != 0) {
    PBL_LOG_ERR("Interrupt status access failed (%d)", ret);
  } else {
    if (status & GH3026_INT_CHIP_RESET) {
      PBL_LOG_WRN("Unexpected chip reset");
    }

    if (gh->ppg.state->running && (status & (GH3026_INT_FIFO_WATERMARK | GH3026_INT_FIFO_FULL))) {
      prv_drain(gh);
    }
  }

  (void)prv_finish(gh, 0);

  pbl_ppg_unlock(&gh->ppg);
}

static void prv_irq_handler(void *arg) {
  new_timer_add_work_callback_from_isr(prv_irq_work, arg);
}

static int prv_init_irq(const struct pbl_gh3026 *gh) {
  const struct pbl_gh3026_config *config = &gh->config;
  int ret;

  ret = pbl_device_init(&config->irq.intc->dev);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_device_init(&config->irq_input.port->dev);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_gpio_configure(&config->irq_input, PBL_GPIO_INPUT);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_intc_request(&config->irq, prv_irq_handler, (void *)gh);
  if (ret != 0) {
    return ret;
  }

  return pbl_intc_enable(&config->irq);
}

static int prv_init(const struct pbl_ppg *ppg) {
  const struct pbl_gh3026 *gh = prv_dev(ppg);
  const struct pbl_gh3026_config *config = &gh->config;
  struct pbl_gh3026_state *state = gh->state;
  int ret;

  if ((config->num_slots == 0U) || (config->num_slots > PBL_PPG_MAX_SLOTS)) {
    return -EINVAL;
  }

  if (pbl_gpio_is_connected(&config->reset)) {
    ret = pbl_device_init(&config->reset.port->dev);
    if (ret != 0) {
      return ret;
    }

    ret = pbl_gpio_configure(&config->reset, PBL_GPIO_OUTPUT_INACTIVE);
    if (ret != 0) {
      return ret;
    }
  }

  ret = prv_reset(gh);
  if (ret != 0) {
    return ret;
  }

  ret = prv_finish(gh, prv_init_awake(gh));
  if (ret != 0) {
    return ret;
  }

  state->info = (struct pbl_ppg_info){
    .slots = config->slots,
    .num_slots = config->num_slots,
    .num_rx = GH3026_NUM_RX,
    .max_led_current_ua = MIN(state->led_fs_ua[0], state->led_fs_ua[1]),
    .gains_ohm = s_gains_ohm,
    .num_gains = ARRAY_LENGTH(s_gains_ohm),
    .full_scale = GH3026_FULL_SCALE,
    .full_scale_uv = GH3026_FULL_SCALE_UV,
    .max_sample_rate_hz = (uint16_t)state->base_rate_hz,
    .fifo_depth = GH3026_FIFO_WORDS,
  };

  PBL_LOG_DBG("base rate %" PRIu32 " Hz, LED ecode %d", state->base_rate_hz, state->led_ecode);

  return prv_init_irq(gh);
}

static const struct pbl_ppg_info *prv_get_info(const struct pbl_ppg *ppg) {
  return &prv_dev(ppg)->state->info;
}

static int prv_write_sequence(const struct pbl_gh3026 *gh, const uint8_t *seq, uint8_t len) {
  int ret;

  for (uint8_t pos = 0U; pos < PBL_PPG_MAX_SLOTS; pos += 2U) {
    uint8_t even = (pos < len) ? seq[pos] : GH3026_SLOT_SEQ_END;
    uint8_t odd = ((pos + 1U) < len) ? seq[pos + 1U] : GH3026_SLOT_SEQ_END;

    ret = prv_write(
        gh, GH3026_SLOT_SEQ(pos),
        PBL_FIELD_PREP(GH3026_SLOT_SEQ_EVEN, even) | PBL_FIELD_PREP(GH3026_SLOT_SEQ_ODD, odd));
    if (ret != 0) {
      return ret;
    }
  }

  for (uint8_t pos = 0U; pos < len; pos++) {
    ret = prv_write(gh, GH3026_SLOT_TIMER(pos), gh->state->slot_timers[seq[pos]]);
    if (ret != 0) {
      return ret;
    }
  }

  return 0;
}

static int prv_configure_awake(const struct pbl_gh3026 *gh, const struct pbl_ppg_config *config,
                               uint32_t sr_mult) {
  uint8_t seq[PBL_PPG_MAX_SLOTS];
  uint8_t len = 0U;
  uint16_t slot_en = 0U;
  uint32_t watermark;
  int ret;

  for (uint8_t slot = 0U; slot < gh->config.num_slots; slot++) {
    uint8_t rx_mask = config->rx_mask[slot];
    uint16_t ctrl0 = PBL_FIELD_PREP(GH3026_SLOT_CTRL0_SR_MULT, sr_mult - 1U);

    for (uint8_t rx = 0U; rx < GH3026_NUM_RX; rx++) {
      if (rx_mask & PBL_BIT(rx)) {
        ctrl0 |= GH3026_SLOT_CTRL0_RX_EN(rx);
      }
    }

    if (rx_mask != 0U) {
      ctrl0 |= GH3026_SLOT_CTRL0_FIFO_EN;
      slot_en |= PBL_BIT(slot);
      seq[len++] = slot;
    }

    ret = prv_update(
        gh, GH3026_SLOT_REG(slot, GH3026_SLOT_CTRL0),
        GH3026_SLOT_CTRL0_RX_EN_ALL | GH3026_SLOT_CTRL0_FIFO_EN | GH3026_SLOT_CTRL0_SR_MULT, ctrl0);
    if (ret != 0) {
      return ret;
    }
  }

  ret = prv_write_sequence(gh, seq, len);
  if (ret != 0) {
    return ret;
  }

  ret = prv_write(gh, GH3026_SLOT_EN, slot_en);
  if (ret != 0) {
    return ret;
  }

  watermark = CLIP((uint32_t)config->batch * gh->state->period_samples, GH3026_WATERMARK_MIN,
                   GH3026_FIFO_WORDS / 2U);

  return prv_write(gh, GH3026_FIFO_WATERMARK, (uint16_t)watermark);
}

static int prv_configure(const struct pbl_ppg *ppg, const struct pbl_ppg_config *config) {
  const struct pbl_gh3026 *gh = prv_dev(ppg);
  struct pbl_gh3026_state *state = gh->state;
  uint32_t sr_mult = state->base_rate_hz / config->sample_rate_hz;
  uint16_t period_samples = 0U;
  int ret;

  if ((sr_mult == 0U) || (sr_mult > GH3026_SR_MULT_MAX)) {
    return -EINVAL;
  }

  for (uint8_t slot = 0U; slot < PBL_PPG_MAX_SLOTS; slot++) {
    period_samples += __builtin_popcount(config->rx_mask[slot]);
  }

  state->period_samples = period_samples;
  state->sample_rate_hz = (uint16_t)(state->base_rate_hz / sr_mult);

  ret = prv_wakeup(gh);
  if (ret != 0) {
    return ret;
  }

  return prv_finish(gh, prv_configure_awake(gh, config, sr_mult));
}

static int prv_start_awake(const struct pbl_gh3026 *gh) {
  int ret;

  ret = prv_write(gh, GH3026_INT_STATUS, GH3026_INT_ALL);
  if (ret != 0) {
    return ret;
  }

  return prv_update(gh, GH3026_CTRL, GH3026_CTRL_SAMPLE_EN, GH3026_CTRL_SAMPLE_EN);
}

static int prv_start(const struct pbl_ppg *ppg) {
  const struct pbl_gh3026 *gh = prv_dev(ppg);
  int ret;

  ret = prv_wakeup(gh);
  if (ret != 0) {
    return ret;
  }

  return prv_finish(gh, prv_start_awake(gh));
}

static int prv_stop(const struct pbl_ppg *ppg) {
  const struct pbl_gh3026 *gh = prv_dev(ppg);
  int ret;

  ret = prv_wakeup(gh);
  if (ret != 0) {
    return ret;
  }

  return prv_finish(gh, prv_update(gh, GH3026_CTRL, GH3026_CTRL_SAMPLE_EN, 0U));
}

static int prv_set_led_current(const struct pbl_ppg *ppg, uint8_t slot, uint8_t led, uint32_t ua) {
  const struct pbl_gh3026 *gh = prv_dev(ppg);
  uint32_t code =
      MIN(((uint64_t)ua * GH3026_DRV_CODE_MAX) / gh->state->led_fs_ua[led], GH3026_DRV_CODE_MAX);
  int ret;

  ret = prv_wakeup(gh);
  if (ret != 0) {
    return ret;
  }

  return prv_finish(gh, prv_update(gh, GH3026_SLOT_REG(slot, GH3026_SLOT_CTRL10 + 2U * led),
                                   GH3026_SLOT_CTRL_DRV_CODE, (uint16_t)code));
}

static int prv_set_gain(const struct pbl_ppg *ppg, uint8_t slot, uint8_t rx, uint32_t ohm) {
  const struct pbl_gh3026 *gh = prv_dev(ppg);
  uint16_t code = 0U;
  int ret;

  while (s_gains_ohm[code] != ohm) {
    code++;
  }

  ret = prv_wakeup(gh);
  if (ret != 0) {
    return ret;
  }

  return prv_finish(gh, prv_update(gh, GH3026_SLOT_REG(slot, GH3026_SLOT_CTRL4),
                                   GH3026_SLOT_CTRL4_GAIN(rx), (uint16_t)(code << (4U * rx))));
}

static uint32_t prv_calibrated_ua(const struct pbl_gh3026 *gh, uint8_t led, uint32_t code) {
  uint32_t fs_ua = gh->state->led_fs_ua[led];
  uint32_t n = 1U;
  int64_t den;

  if (code == 0U) {
    return 0U;
  }

  for (uint32_t fs = fs_ua; fs < s_led_fs_ua[GH3026_LED_FS_MAX_CODE]; fs *= 2U) {
    n *= 2U;
  }

  den = ((GH3026_CAL_ECODE_DEN + gh->state->led_ecode) * (int64_t)GH3026_DRV_CODE_MAX * n *
         GH3026_CAL_SCALE) /
            ((int64_t)GH3026_CAL_ECODE_DEN * code) +
        s_cal_y_micro[led];

  return (uint32_t)((GH3026_CAL_V_UV * GH3026_CAL_SCALE) / den);
}

static int prv_get_channel_awake(const struct pbl_gh3026 *gh, uint8_t slot, uint8_t rx,
                                 struct pbl_ppg_channel *channel) {
  const struct pbl_ppg_slot *s = &gh->config.slots[slot];
  uint32_t code;
  uint16_t reg;
  int ret;

  ret = prv_read(gh, GH3026_SLOT_REG(slot, GH3026_SLOT_CTRL4), &reg);
  if (ret != 0) {
    return ret;
  }

  channel->gain_ohm = s_gains_ohm[MIN(PBL_FIELD_GET(GH3026_SLOT_CTRL4_GAIN(rx), reg),
                                      ARRAY_LENGTH(s_gains_ohm) - 1U)];
  channel->calibrated_gain_ohm = channel->gain_ohm;

  for (uint8_t led = 0U; led < GH3026_NUM_LED_DRIVERS; led++) {
    if ((s->led_mask & PBL_BIT(led)) == 0U) {
      continue;
    }

    ret = prv_read(gh, GH3026_SLOT_REG(slot, GH3026_SLOT_CTRL10 + 2U * led), &reg);
    if (ret != 0) {
      return ret;
    }

    code = PBL_FIELD_GET(GH3026_SLOT_CTRL_DRV_CODE, reg);
    channel->led_current_ua[led] =
        (code * gh->state->led_fs_ua[led] + GH3026_DRV_CODE_MAX - 1U) / GH3026_DRV_CODE_MAX;
    channel->calibrated_led_current_ua[led] = prv_calibrated_ua(gh, led, code);
  }

  return 0;
}

static int prv_get_channel(const struct pbl_ppg *ppg, uint8_t slot, uint8_t rx,
                           struct pbl_ppg_channel *channel) {
  const struct pbl_gh3026 *gh = prv_dev(ppg);
  int ret;

  ret = prv_wakeup(gh);
  if (ret != 0) {
    return ret;
  }

  return prv_finish(gh, prv_get_channel_awake(gh, slot, rx, channel));
}

static void prv_decode(const struct pbl_gh3026 *gh, uint16_t num_words) {
  struct pbl_gh3026_state *state = gh->state;

  for (uint16_t i = 0U; i < num_words; i++) {
    const uint8_t *b = &state->buf[i * sizeof(uint32_t)];
    uint32_t word = ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) | ((uint32_t)b[2] << 8) | b[3];
    int32_t value = (int32_t)PBL_FIELD_GET(GH3026_FIFO_DATA, word) - GH3026_FIFO_ZERO;
    uint8_t flags = 0U;

    if (PBL_FIELD_GET(GH3026_FIFO_ADJ, word) != 0U) {
      flags |= PBL_PPG_SAMPLE_GAIN_CHANGED;
    }

    if ((value >= (GH3026_FULL_SCALE - GH3026_SATURATION_MARGIN)) ||
        (value <= -(GH3026_FULL_SCALE - GH3026_SATURATION_MARGIN))) {
      flags |= PBL_PPG_SAMPLE_SATURATED;
    }

    state->samples[i] = (struct pbl_ppg_sample){
      .value = value,
      .slot = (uint8_t)PBL_FIELD_GET(GH3026_FIFO_SLOT, word),
      .rx = (uint8_t)PBL_FIELD_GET(GH3026_FIFO_RX, word),
      .flags = flags,
    };
  }
}

static void prv_drain(const struct pbl_gh3026 *gh) {
  static const uint8_t fifo_addr[] = {GH3026_FIFO_ADDR >> 8, GH3026_FIFO_ADDR & 0xFFU};
  struct pbl_gh3026_state *state = gh->state;
  uint64_t now_ms = pbl_uptime_ms();
  uint16_t used;
  uint32_t remaining;
  int ret;

  ret = prv_read(gh, GH3026_INT_FIFO_USED, &used);
  if (ret != 0) {
    PBL_LOG_ERR("FIFO count read failed (%d)", ret);
    return;
  }

  remaining = MIN(used, GH3026_FIFO_WORDS);

  while (remaining > 0U) {
    uint16_t n = (uint16_t)MIN(remaining, PBL_GH3026_READ_WORDS);
    struct pbl_ppg_batch batch;
    uint32_t later_periods;

    ret = pbl_i2c_write_read(&gh->i2c, fifo_addr, sizeof(fifo_addr), state->buf,
                             n * sizeof(uint32_t));
    if (ret != 0) {
      PBL_LOG_ERR("FIFO read failed (%d)", ret);
      return;
    }

    remaining -= n;
    later_periods = remaining / MAX(state->period_samples, 1U);

    prv_decode(gh, n);

    batch = (struct pbl_ppg_batch){
      .samples = state->samples,
      .num_samples = n,
      .sample_rate_hz = state->sample_rate_hz,
      .timestamp_ms = now_ms - (later_periods * PBL_MSEC_PER_SEC) / state->sample_rate_hz,
    };

    pbl_ppg_report_samples(&gh->ppg, &batch);
  }
}

const struct pbl_ppg_ops pbl_gh3026_ops = {
  .init = prv_init,
  .get_info = prv_get_info,
  .configure = prv_configure,
  .start = prv_start,
  .stop = prv_stop,
  .set_led_current = prv_set_led_current,
  .set_gain = prv_set_gain,
  .get_channel = prv_get_channel,
};
