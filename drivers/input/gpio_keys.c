/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/drivers/input/gpio_keys.h>
#include <pbl/input/input.h>
#include <pbl/util/misc.h>

static void prv_debounced(struct pbl_timer *timer) {
  struct pbl_gpio_keys_key_state *state =
      container_of(timer, struct pbl_gpio_keys_key_state, timer);
  int pressed = pbl_gpio_get(&state->key->gpio);

  if (pressed < 0 || (pressed != 0) == state->pressed) {
    return;
  }

  state->pressed = pressed != 0;
  pbl_input_report_key(state->key->code, state->pressed, true);
}

static void prv_edge(void *arg) {
  struct pbl_gpio_keys_key_state *state = arg;

  pbl_timer_start(&state->timer, PBL_MSEC(state->key->debounce_interval_ms));
}

static int prv_key_init(const struct pbl_gpio_keys_key *key,
                        struct pbl_gpio_keys_key_state *state) {
  struct pbl_irq_spec irq;
  int ret;

  ret = pbl_device_init(&key->gpio.port->dev);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_gpio_configure(&key->gpio, PBL_GPIO_INPUT);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_gpio_to_irq(&key->gpio, PBL_IRQ_TYPE_EDGE_BOTH, &irq);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_device_init(&irq.intc->dev);
  if (ret != 0) {
    return ret;
  }

  state->key = key;
  state->pressed = false;
  pbl_timer_init(&state->timer, prv_debounced);

  ret = pbl_intc_request(&irq, prv_edge, state);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_intc_enable(&irq);
  if (ret != 0) {
    return ret;
  }

  prv_edge(state);

  return 0;
}

int pbl_gpio_keys_init(const struct pbl_device *dev) {
  const struct pbl_gpio_keys *keys = container_of(dev, const struct pbl_gpio_keys, dev);
  int ret;

  for (uint8_t i = 0U; i < keys->num_keys; i++) {
    ret = prv_key_init(&keys->keys[i], &keys->state[i]);
    if (ret != 0) {
      return ret;
    }
  }

  return 0;
}
