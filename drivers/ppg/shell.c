/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <inttypes.h>

#include <pbl/drivers/ppg.h>
#include <pbl/kernel/sem.h>
#include <pbl/shell/shell.h>
#include <pbl/util/size.h>

#define READ_MAX_SAMPLES 64U
#define READ_RATE_HZ     25U
#define READ_TIMEOUT_MS  5000U

static const char *const s_colors[] = {
  [PBL_PPG_LED_GREEN] = "green",
  [PBL_PPG_LED_RED] = "red",
  [PBL_PPG_LED_IR] = "ir",
};

static struct {
  struct pbl_sem done;
  struct pbl_ppg_sample samples[READ_MAX_SAMPLES];
  uint16_t num_samples;
  uint16_t wanted;
} s_read = {
  .done = PBL_SEM_INITIALIZER(0, 1),
};

static int prv_print_channel(const struct pbl_shell *sh, uint8_t slot, uint8_t rx) {
  struct pbl_ppg_channel channel;
  int ret;

  ret = pbl_ppg_get_channel(PPG, slot, rx, &channel);
  if (ret != 0) {
    pbl_shell_error(sh, "slot %u rx %u: read failed (%d)", slot, rx, ret);
    return ret;
  }

  pbl_shell_print(sh, "  rx%u: gain %" PRIu32 " ohm (%" PRIu32 " calibrated)", rx, channel.gain_ohm,
                  channel.calibrated_gain_ohm);
  pbl_shell_print(sh,
                  "  led0 %" PRIu32 " uA (%" PRIu32 " calibrated), led1 %" PRIu32 " uA (%" PRIu32
                  " calibrated)",
                  channel.led_current_ua[0], channel.calibrated_led_current_ua[0],
                  channel.led_current_ua[1], channel.calibrated_led_current_ua[1]);

  return 0;
}

static int prv_info(const struct pbl_shell *sh, size_t argc, char **argv) {
  const struct pbl_ppg_info *info = pbl_ppg_get_info(PPG);
  int ret;

  if (info == nullptr) {
    pbl_shell_error(sh, "not ready");
    return -ENODEV;
  }

  pbl_shell_print(sh, "%s: %u slots, %u receivers, %s", PPG->dev.name, info->num_slots,
                  info->num_rx, pbl_ppg_is_running(PPG) ? "running" : "stopped");
  pbl_shell_print(sh, "max LED current %" PRIu32 " uA, max rate %u Hz, FIFO %u",
                  info->max_led_current_ua, info->max_sample_rate_hz, info->fifo_depth);

  for (uint8_t slot = 0U; slot < info->num_slots; slot++) {
    const struct pbl_ppg_slot *s = &info->slots[slot];

    pbl_shell_print(
        sh, "slot %u:%s%s%s%s%s", slot, (s->led_mask == 0U) ? " ambient" : "",
        (s->led_mask & 0x1U) ? " led0 " : "", (s->led_mask & 0x1U) ? s_colors[s->leds[0]] : "",
        (s->led_mask & 0x2U) ? " led1 " : "", (s->led_mask & 0x2U) ? s_colors[s->leds[1]] : "");
    for (uint8_t rx = 0U; rx < info->num_rx; rx++) {
      if ((s->rx_mask & (1U << rx)) == 0U) {
        continue;
      }

      ret = prv_print_channel(sh, slot, rx);
      if (ret != 0) {
        return ret;
      }
    }
  }

  return 0;
}

static void prv_read_samples(const struct pbl_ppg *ppg, const struct pbl_ppg_batch *batch,
                             void *user_data) {
  for (uint16_t i = 0U; (i < batch->num_samples) && (s_read.num_samples < s_read.wanted); i++) {
    s_read.samples[s_read.num_samples++] = batch->samples[i];
  }

  if (s_read.num_samples == s_read.wanted) {
    pbl_sem_give(&s_read.done);
  }
}

static const struct pbl_ppg_listener s_read_listener = {.samples = prv_read_samples};

static int prv_capture(const struct pbl_ppg_info *info) {
  struct pbl_ppg_config config = {.sample_rate_hz = READ_RATE_HZ, .batch = 1U};
  int ret;

  for (uint8_t slot = 0U; slot < info->num_slots; slot++) {
    config.rx_mask[slot] = info->slots[slot].rx_mask;
  }

  ret = pbl_ppg_configure(PPG, &config);
  if (ret != 0) {
    return ret;
  }

  pbl_sem_reset(&s_read.done);
  s_read.num_samples = 0U;

  ret = pbl_ppg_start(PPG);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_sem_take(&s_read.done, PBL_MSEC(READ_TIMEOUT_MS));

  (void)pbl_ppg_stop(PPG);

  return ret;
}

static int prv_read(const struct pbl_shell *sh, size_t argc, char **argv) {
  const struct pbl_ppg_info *info = pbl_ppg_get_info(PPG);
  unsigned long n = 1UL;
  int ret;

  if (info == nullptr) {
    pbl_shell_error(sh, "not ready");
    return -ENODEV;
  }

  if ((argc > 1) &&
      ((pbl_shell_strtoul(argv[1], &n) != 0) || (n == 0UL) || (n > READ_MAX_SAMPLES))) {
    pbl_shell_error(sh, "invalid count (1-%u)", READ_MAX_SAMPLES);
    return -EINVAL;
  }

  if (pbl_ppg_is_running(PPG)) {
    pbl_shell_error(sh, "busy");
    return -EBUSY;
  }

  s_read.wanted = (uint16_t)n;
  (void)pbl_ppg_set_listener(PPG, &s_read_listener);
  ret = prv_capture(info);
  (void)pbl_ppg_set_listener(PPG, nullptr);

  if (ret != 0) {
    pbl_shell_error(sh, "read failed (%d)", ret);
    return ret;
  }

  for (uint16_t i = 0U; i < s_read.num_samples; i++) {
    const struct pbl_ppg_sample *sample = &s_read.samples[i];

    pbl_shell_print(sh, "slot %u rx %u: %" PRId32 "%s", sample->slot, sample->rx, sample->value,
                    (sample->flags & PBL_PPG_SAMPLE_SATURATED) ? " (saturated)" : "");
  }

  return 0;
}

static int prv_led(const struct pbl_shell *sh, size_t argc, char **argv) {
  unsigned long slot;
  unsigned long led;
  unsigned long ua;
  int ret;

  if ((argc != 4) || (pbl_shell_strtoul(argv[1], &slot) != 0) ||
      (pbl_shell_strtoul(argv[2], &led) != 0) || (pbl_shell_strtoul(argv[3], &ua) != 0) ||
      (slot > UINT8_MAX) || (led > UINT8_MAX) || (ua > UINT32_MAX)) {
    pbl_shell_error(sh, "usage: ppg led <slot> <led> <uA>");
    return -EINVAL;
  }

  ret = pbl_ppg_set_led_current(PPG, (uint8_t)slot, (uint8_t)led, (uint32_t)ua);
  if (ret != 0) {
    pbl_shell_error(sh, "failed (%d)", ret);
  }

  return ret;
}

static int prv_gain(const struct pbl_shell *sh, size_t argc, char **argv) {
  unsigned long slot;
  unsigned long rx;
  unsigned long ohm;
  int ret;

  if ((argc != 4) || (pbl_shell_strtoul(argv[1], &slot) != 0) ||
      (pbl_shell_strtoul(argv[2], &rx) != 0) || (pbl_shell_strtoul(argv[3], &ohm) != 0) ||
      (slot > UINT8_MAX) || (rx > UINT8_MAX) || (ohm > UINT32_MAX)) {
    pbl_shell_error(sh, "usage: ppg gain <slot> <rx> <ohm>");
    return -EINVAL;
  }

  ret = pbl_ppg_set_gain(PPG, (uint8_t)slot, (uint8_t)rx, (uint32_t)ohm);
  if (ret != 0) {
    pbl_shell_error(sh, "failed (%d)", ret);
  }

  return ret;
}

static const struct pbl_shell_cmd sub_ppg[] = {
  PBL_SHELL_CMD(info, nullptr, "Show the front end and channel settings", prv_info),
  PBL_SHELL_CMD(read, nullptr, "Sample all slots: read [n]", prv_read),
  PBL_SHELL_CMD(led, nullptr, "Set an LED current: led <slot> <led> <uA>", prv_led),
  PBL_SHELL_CMD(gain, nullptr, "Set a receiver gain: gain <slot> <rx> <ohm>", prv_gain),
  PBL_SHELL_SUBCMD_SET_END,
};

PBL_SHELL_CMD_REGISTER(ppg, sub_ppg, "PPG front end", nullptr);
