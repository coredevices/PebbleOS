/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

typedef void* GPIO_TypeDef;
#define GPIO_Port_NULL ((GPIO_TypeDef*) 0)
#define GPIOA          ((GPIO_TypeDef*) 1)

enum {
  GPIO_Pin_1,
  GPIO_Pin_2
};

typedef enum {
  GPIO_OType_PP,
  GPIO_OType_OD
} GPIOOType_TypeDef;

typedef void* GPIOSpeed_TypeDef;

typedef enum {
  GPIO_PuPd_NOPULL,
  GPIO_PuPd_UP,
  GPIO_PuPd_DOWN
} GPIOPuPd_TypeDef;

typedef struct {
} AfConfig;

typedef struct {
} OutputConfig;

typedef struct {
  GPIO_TypeDef* const gpio; ///< One of GPIOX. For example, GPIOA.
  const uint32_t gpio_pin; ///< One of GPIO_Pin_X.
} InputConfig;

typedef struct {
  const InputConfig fpc_pinstrap_1;
  const InputConfig fpc_pinstrap_2;
} BoardConfig;

static const BoardConfig BOARD_CONFIG = {
  .fpc_pinstrap_1 = { GPIOA, GPIO_Pin_1 },
  .fpc_pinstrap_2 = { GPIOA, GPIO_Pin_2 },
};
