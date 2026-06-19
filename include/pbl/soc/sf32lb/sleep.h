/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

//! Sleep levels, ordered from shallowest to deepest.
typedef enum {
  SOC_SF32LB_ACTIVE = 0,  //!< No sleep at all
  SOC_SF32LB_WFI,         //!< Light WFI
  SOC_SF32LB_DEEPWFI,     //!< Deep WFI
  SOC_SF32LB_DEEPSLEEP,   //!< Deep sleep
} SocSf32lbSleepLevel;

//! Block the given sleep level and every deeper level. For example,
//! soc_sf32lb_sleep_block(SOC_SF32LB_DEEPWFI) forbids deep WFI and deep sleep,
//! leaving plain WFI as the deepest permitted level. SOC_SF32LB_ACTIVE cannot
//! be blocked. Refcounted; balance each call with soc_sf32lb_sleep_release().
//! Safe to call concurrently. With no blocks, the deepest permitted level is
//! SOC_SF32LB_DEEPSLEEP.
void soc_sf32lb_sleep_block(SocSf32lbSleepLevel level);

//! Release a block taken with soc_sf32lb_sleep_block(level).
void soc_sf32lb_sleep_release(SocSf32lbSleepLevel level);

//! Deepest sleep level currently permitted (one step shallower than the
//! shallowest outstanding block).
SocSf32lbSleepLevel soc_sf32lb_sleep_max_level(void);
