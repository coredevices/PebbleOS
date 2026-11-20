/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/power_domain.h>

/**
 * @defgroup drivers_power_domain_sf32lb_pmuc SF32LB PMUC power domains
 * @ingroup drivers_power_domain
 * @brief @ref drivers_power_domain provider for the HPSYS domain of the SF32LB PMUC.
 * @{
 */

/** @cond INTERNAL_HIDDEN */
#define PBL_PD_SF32LB_PMUC__NUM_DOMAINS 1U

struct pbl_pd_sf32lb_pmuc {
  struct pbl_power_domain_provider provider;
  uintptr_t base;
  uintptr_t cfg;
};

extern const struct pbl_power_domain_provider_ops pbl_pd_sf32lb_pmuc_ops;
/** @endcond */

/**
 * @brief Define the PMUC power domain provider.
 *
 * @param sym Symbol of the provider.
 * @param _name Name.
 * @param _base PMUC registers.
 * @param _cfg HPSYS_CFG registers.
 */
#define PBL_PD_SF32LB_PMUC_DEFINE(sym, _name, _base, _cfg)                                     \
  PBL_POWER_DOMAIN_PROVIDER_STATE_DEFINE(sym, PBL_PD_SF32LB_PMUC__NUM_DOMAINS);                \
  PBL_DEVICE_INSTANCE(sym, struct pbl_pd_sf32lb_pmuc, struct pbl_power_domain_provider,        \
                      provider) {                                                              \
    .provider =                                                                                \
        PBL_POWER_DOMAIN_PROVIDER_INIT(sym, _name, nullptr, nullptr, &pbl_pd_sf32lb_pmuc_ops), \
    .base = (_base), .cfg = (_cfg),                                                            \
  }

/** @} */
