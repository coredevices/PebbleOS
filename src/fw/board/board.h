/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#if defined(CONFIG_QEMU)
# include "board_qemu.h"
#elif defined(CONFIG_SOC_NRF52)
# include "board_nrf5.h"
#elif defined(CONFIG_SOC_SF32LB52)
# include "board_sf32lb52.h"
#elif !defined(SDK) && !defined(UNITTEST)
# error "Unknown or missing MICRO_FAMILY_* define"
#endif
