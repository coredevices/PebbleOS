/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <pbl/drivers/ppg.h>
#include <pbl/services/hrm/hrm_ppg.h>

#define HRM_AGC_MAX_RX 2

struct hrm_agc_window {
  uint32_t count;
  uint32_t drop;
  int64_t sum;
  int32_t max;
  int32_t min;
  int32_t history[2];
  uint8_t history_len;
};

struct hrm_agc_slot {
  const struct hrm_ppg_slot_config *config;
  uint16_t rate_hz;
  uint8_t main_rx;
  uint32_t led_ua;
  uint8_t gain_idx[HRM_AGC_MAX_RX];
  uint8_t max_gain_idx;
  struct hrm_agc_window window[HRM_AGC_MAX_RX];
  bool adjusted;
  uint32_t settle_ms;
  bool recentered;
  uint8_t recenter_count;
  uint32_t sat_count;
  uint32_t sat_samples;
  bool saturated;
  bool prev_mean_valid;
  int32_t prev_mean;
};

int hrm_agc_init(struct hrm_agc_slot *agc, const struct pbl_ppg *ppg,
                 const struct hrm_ppg_slot_config *config, uint16_t rate_hz);

void hrm_agc_sample(struct hrm_agc_slot *agc, const struct pbl_ppg *ppg, uint8_t rx, int32_t value,
                    uint32_t time_ms);
