/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/input/adc_keys.h>
#include <pbl/input/input.h>
#include <pbl/util/misc.h>

// Time the line must be quiet before it is read
#define SETTLE_MS 20U

// The key nearest to the voltage, NULL if key-up is
static const struct pbl_adc_keys_key *prv_nearest(const struct pbl_adc_keys *keys, int32_t uv) {
  const struct pbl_adc_keys_key *nearest = nullptr;
  int64_t best = llabs((int64_t)uv - keys->keyup_uv);

  for (uint8_t i = 0U; i < keys->num_keys; i++) {
    int64_t diff = llabs((int64_t)uv - keys->keys[i].press_uv);

    if (diff < best) {
      best = diff;
      nearest = &keys->keys[i];
    }
  }

  return nearest;
}

static void prv_scan(struct pbl_work *work) {
  struct pbl_adc_keys_state *state = container_of(work, struct pbl_adc_keys_state, scan.work);
  const struct pbl_adc_keys *keys = state->keys;
  int32_t uv;

  if (pbl_adc_read(&keys->channel, &uv) == 0) {
    const struct pbl_adc_keys_key *key = prv_nearest(keys, uv);

    if (key != state->pressed) {
      if (state->pressed != nullptr) {
        pbl_input_report_key(state->pressed->code, false, key == nullptr);
      }
      if (key != nullptr) {
        pbl_input_report_key(key->code, true, true);
      }
      state->pressed = key;
    }
  }
}

static void prv_edge(void *arg) {
  struct pbl_adc_keys_state *state = arg;

  (void)pbl_delayed_work_reschedule(&pbl_sys_workq, &state->scan, PBL_MSEC(SETTLE_MS));
}

int pbl_adc_keys_init(const struct pbl_device *dev) {
  const struct pbl_adc_keys *keys = container_of(dev, const struct pbl_adc_keys, dev);
  struct pbl_adc_keys_state *state = keys->state;
  int ret;

  ret = pbl_device_init(&keys->channel.adc->dev);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_device_init(&keys->irq.intc->dev);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_pinctrl_select_state(&keys->pinctrl, PBL_PINCTRL_STATE_DEFAULT);
  if (ret != 0 && ret != -ENOENT) {
    return ret;
  }

  state->keys = keys;
  state->pressed = nullptr;
  pbl_delayed_work_init(&state->scan, prv_scan);

  ret = pbl_intc_request(&keys->irq, prv_edge, state);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_intc_enable(&keys->irq);
  if (ret != 0) {
    return ret;
  }

  // The keys as they are now
  (void)pbl_delayed_work_schedule(&pbl_sys_workq, &state->scan, PBL_MSEC(SETTLE_MS));

  return 0;
}

#include PBL_DT_INSTANCES(adc_keys_irq)
