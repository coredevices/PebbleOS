/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/input/rotary_encoder.h>
#include <pbl/drivers/intc.h>
#include <pbl/input/input.h>
#include <pbl/util/misc.h>

// Gray code states per period, one transition per quarter
#define QUARTERS_PER_PERIOD 4

// Quarter steps of a transition, indexed by (previous state << 2) | state
static const int8_t s_transition[16] = {
  0, 1, 0, -1, -1, 0, 1, 0, 0, -1, 0, 1, 1, 0, -1, 0,
};

static int prv_read(const struct pbl_rotary_encoder *enc) {
  int a = pbl_gpio_get(&enc->a);
  int b = pbl_gpio_get(&enc->b);

  if (a < 0) {
    return a;
  }

  if (b < 0) {
    return b;
  }

  // Gray to binary order: 00, 01, 11, 10 become 0, 1, 2, 3
  return (a << 1) | (a ^ b);
}

static void prv_edge(void *arg) {
  const struct pbl_rotary_encoder *enc = arg;
  struct pbl_rotary_encoder_state *state = enc->state;
  int quarters = QUARTERS_PER_PERIOD / enc->steps_per_period;
  int cur = prv_read(enc);
  int steps;

  if (cur < 0 || cur == state->last) {
    return;
  }

  state->acc += s_transition[(state->last << 2) | cur];
  state->last = cur;

  // Steps end on the states that are multiples of a step: report the quarters moved since the
  // previous one there, rounded, and start over, so a missed state does not shift later steps
  if (cur % quarters != 0) {
    return;
  }

  steps = (state->acc + ((state->acc < 0) ? -(quarters / 2) : quarters / 2)) / quarters;
  state->acc = 0;
  if (steps != 0) {
    pbl_input_report_rel(enc->code, steps, true);
  }
}

static int prv_line_init(const struct pbl_gpio *gpio) {
  int ret;

  ret = pbl_device_init(&gpio->port->dev);
  if (ret != 0) {
    return ret;
  }

  return pbl_gpio_configure(gpio, PBL_GPIO_INPUT);
}

static int prv_line_irq(const struct pbl_rotary_encoder *enc, const struct pbl_gpio *gpio) {
  struct pbl_irq_spec irq;
  int ret;

  ret = pbl_gpio_to_irq(gpio, PBL_IRQ_TYPE_EDGE_BOTH, &irq);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_device_init(&irq.intc->dev);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_intc_request(&irq, prv_edge, (void *)enc);
  if (ret != 0) {
    return ret;
  }

  if (enc->wakeup_source) {
    ret = pbl_intc_set_wake(&irq, PBL_IRQ_TYPE_EDGE_BOTH);
    if (ret != 0) {
      return ret;
    }
  }

  return pbl_intc_enable(&irq);
}

int pbl_rotary_encoder_init(const struct pbl_device *dev) {
  const struct pbl_rotary_encoder *enc =
      PBL_CONTAINER_OF(dev, const struct pbl_rotary_encoder, dev);
  int ret;

  if (enc->steps_per_period != 1U && enc->steps_per_period != 2U && enc->steps_per_period != 4U) {
    return -EINVAL;
  }

  ret = prv_line_init(&enc->a);
  if (ret != 0) {
    return ret;
  }

  ret = prv_line_init(&enc->b);
  if (ret != 0) {
    return ret;
  }

  ret = prv_read(enc);
  if (ret < 0) {
    return ret;
  }

  enc->state->last = ret;
  enc->state->acc = 0;

  ret = prv_line_irq(enc, &enc->a);
  if (ret != 0) {
    return ret;
  }

  return prv_line_irq(enc, &enc->b);
}

#include PBL_DT_INSTANCES(rotary_encoder)
