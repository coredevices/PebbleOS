/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/power_domain.h>
#include <pbl/kernel/assert.h>

PBL_DEVICE_CLASS_DEFINE(pbl_power_domain_provider_class, "power_domain");

int pbl_power_domain_set_performance_state(const struct pbl_power_domain *pd, uint32_t level) {
  const struct pbl_power_domain_provider *provider = pd->provider;
  int ret;

  PBL_ASSERTN(pbl_device_is_ready(&provider->dev));

  if (pd->id >= provider->num_domains) {
    return -EINVAL;
  }

  if (provider->levels[pd->id] == level) {
    return 0;
  }

  ret = provider->ops->set_performance_state(provider, pd->id, level);
  if (ret == 0) {
    provider->levels[pd->id] = level;
  }

  return ret;
}

uint32_t pbl_power_domain_get_performance_state(const struct pbl_power_domain *pd) {
  if (pd->id >= pd->provider->num_domains) {
    return 0U;
  }

  return pd->provider->levels[pd->id];
}
