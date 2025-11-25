/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "board/board.h"
#include "drivers/fpc_pinstrap.h"

static bool s_pin_pull_up_enabled[2] = { false, false };

void gpio_input_init_pull_up_down(const InputConfig *input_cfg, GPIOPuPd_TypeDef pupd) {
  s_pin_pull_up_enabled[input_cfg->gpio_pin] = (pupd == GPIO_PuPd_UP);
}

void gpio_analog_init(const InputConfig *input_cfg) {
}

static enum {
  PinstrapResult_GND,
  PinstrapResult_Vplus,
  PinstrapResult_Float
} s_pinstrap_results[2];

bool gpio_input_read(const InputConfig *input_cfg) {
  switch (s_pinstrap_results[input_cfg->gpio_pin]) {
    case PinstrapResult_GND:
      return false;
    case PinstrapResult_Vplus:
      return true;
    case PinstrapResult_Float:
      return s_pin_pull_up_enabled[input_cfg->gpio_pin];
  }
}

void test_fpc_pinstrap__simple(void) {
  s_pinstrap_results[0] = PinstrapResult_GND;

  s_pinstrap_results[1] = PinstrapResult_GND;
  cl_assert(fpc_pinstrap_get_value() == 0x0);

  s_pinstrap_results[1] = PinstrapResult_Vplus;
  cl_assert(fpc_pinstrap_get_value() == 0x1);

  s_pinstrap_results[1] = PinstrapResult_Float;
  cl_assert(fpc_pinstrap_get_value() == 0x2);


  s_pinstrap_results[0] = PinstrapResult_Vplus;

  s_pinstrap_results[1] = PinstrapResult_GND;
  cl_assert(fpc_pinstrap_get_value() == 0x3);

  s_pinstrap_results[1] = PinstrapResult_Vplus;
  cl_assert(fpc_pinstrap_get_value() == 0x4);

  s_pinstrap_results[1] = PinstrapResult_Float;
  cl_assert(fpc_pinstrap_get_value() == 0x5);


  s_pinstrap_results[0] = PinstrapResult_Float;

  s_pinstrap_results[1] = PinstrapResult_GND;
  cl_assert(fpc_pinstrap_get_value() == 0x6);

  s_pinstrap_results[1] = PinstrapResult_Vplus;
  cl_assert(fpc_pinstrap_get_value() == 0x7);

  s_pinstrap_results[1] = PinstrapResult_Float;
  cl_assert(fpc_pinstrap_get_value() == 0x8);
}
