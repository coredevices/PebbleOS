/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#if defined(MICRO_FAMILY_STM32F4)
# include "board_stm32.h"
#elif defined(MICRO_FAMILY_NRF52840)
# include "board_nrf5.h"
#elif defined(MICRO_FAMILY_SF32LB52)
# include "board_sf32lb52.h"
#elif !defined(SDK) && !defined(UNITTEST)
# error "Unknown or missing MICRO_FAMILY_* define"
#endif
