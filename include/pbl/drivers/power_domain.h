/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/device.h>

/**
 * @defgroup drivers_power_domain Power domain
 * @ingroup drivers
 * @brief Power domain class.
 *
 * A power domain provider is a struct pbl_power_domain_provider device with one or more domains,
 * each identified by a provider-specific number. A domain runs at a performance state, a level
 * whose meaning is the provider's, as the voltage mode its clocks need, as Linux power domains
 * do. A consumer holds a struct pbl_power_domain, lists the provider among its device
 * dependencies, and sets the performance state it needs before it raises its clocks:
 *
 * @code{.c}
 * static const struct pbl_power_domain s_pd = PBL_POWER_DOMAIN(&dt_pmuc, 0);
 *
 * pbl_power_domain_set_performance_state(&s_pd, 4);
 * @endcode
 *
 * Each domain has one consumer: its state is the last one set.
 * @{
 */

struct pbl_power_domain_provider;

/** @brief Power domain provider driver operations. */
struct pbl_power_domain_provider_ops {
  /** Set the performance state of a domain. Returns 0 or a negative errno. */
  int (*set_performance_state)(const struct pbl_power_domain_provider *provider, uint32_t id,
                               uint32_t level);
};

/** @brief A power domain provider. */
struct pbl_power_domain_provider {
  /** Device. */
  struct pbl_device dev;
  /** Driver operations. */
  const struct pbl_power_domain_provider_ops *ops;
  /** Performance state of each domain, 0 until one is set. */
  uint32_t *levels;
  /** Number of domains. */
  uint16_t num_domains;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_device_class pbl_power_domain_provider_class;
/** @endcond */

/**
 * @brief Define the class state of power domain provider @p sym, for the driver's define macro.
 *
 * @param sym Symbol of the provider instance.
 * @param _num_domains Number of domains.
 */
#define PBL_POWER_DOMAIN_PROVIDER_STATE_DEFINE(sym, _num_domains) \
  PBL_DEVICE_STATE_DEFINE(sym);                                   \
  static uint32_t sym##_pd_levels[_num_domains]

/**
 * @brief Initializer for the struct pbl_power_domain_provider of provider @p sym.
 *
 * @param sym Symbol of the provider instance, with its state defined by
 * PBL_POWER_DOMAIN_PROVIDER_STATE_DEFINE().
 * @param _name Name.
 * @param _init Device init function, or NULL.
 * @param _parent Parent device, or NULL.
 * @param _ops Driver operations.
 */
#define PBL_POWER_DOMAIN_PROVIDER_INIT(sym, _name, _init, _parent, _ops) \
  {                                                                      \
    .dev = PBL_DEVICE_INIT_CLASS(sym, _name, _init, _parent, nullptr,    \
                                 &pbl_power_domain_provider_class),      \
    .ops = (_ops),                                                       \
    .levels = sym##_pd_levels,                                           \
    .num_domains = sizeof(sym##_pd_levels) / sizeof(sym##_pd_levels[0]), \
  }

/** @brief A power domain, as used by a consumer. */
struct pbl_power_domain {
  /** Provider. */
  const struct pbl_power_domain_provider *provider;
  /** Domain number on the provider. */
  uint32_t id;
};

/**
 * @brief Initializer for a struct pbl_power_domain.
 *
 * @param _provider Power domain provider.
 * @param _id Domain number, from the provider's binding header.
 */
#define PBL_POWER_DOMAIN(_provider, _id) {.provider = (_provider), .id = (_id)}

/**
 * @brief Set the performance state of a power domain.
 *
 * @param pd Power domain.
 * @param level Performance state, as the provider defines it.
 * @return 0, @c -EINVAL for an unknown domain or state, or another negative errno.
 */
int pbl_power_domain_set_performance_state(const struct pbl_power_domain *pd, uint32_t level);

/**
 * @brief Get the performance state of a power domain.
 *
 * @param pd Power domain.
 * @return The last state set, 0 if none.
 */
uint32_t pbl_power_domain_get_performance_state(const struct pbl_power_domain *pd);

/** @} */
