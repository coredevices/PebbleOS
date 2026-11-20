/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/util/bits.h>

/* Register offsets of a GPIO port */

#define STM32_GPIO_MODER_OFFSET   0x0000
#define STM32_GPIO_OTYPER_OFFSET  0x0004
#define STM32_GPIO_OSPEEDR_OFFSET 0x0008
#define STM32_GPIO_PUPDR_OFFSET   0x000c
#define STM32_GPIO_AFR_OFFSET(n)  (0x0020 + ((n) / 8U) * 4U)

#define STM32_GPIO_PORT_SIZE 0x400U
#define STM32_GPIO_NUM_PINS  16U

/* Register fields of pin n */

#define GPIO_MODER_MASK(n)   (0x3U << ((n) * 2U))
#define GPIO_MODER_INPUT     0U
#define GPIO_MODER_OUTPUT    1U
#define GPIO_MODER_AF        2U
#define GPIO_MODER_ANALOG    3U
#define GPIO_OSPEEDR_MASK(n) (0x3U << ((n) * 2U))
#define GPIO_PUPDR_MASK(n)   (0x3U << ((n) * 2U))
#define GPIO_PUPDR_NONE      0U
#define GPIO_PUPDR_UP        1U
#define GPIO_PUPDR_DOWN      2U
#define GPIO_AFR_MASK(n)     (0xfU << (((n) % 8U) * 4U))
