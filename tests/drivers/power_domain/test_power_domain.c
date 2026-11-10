/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <string.h>

#include <pbl/drivers/power_domain.h>

#include <clar.h>

static uint32_t s_calls;
static uint32_t s_last_id;
static uint32_t s_last_level;
static int s_ret;

static int prv_set(const struct pbl_power_domain_provider *provider, uint32_t id, uint32_t level) {
  s_calls++;
  s_last_id = id;
  s_last_level = level;
  return s_ret;
}

static const struct pbl_power_domain_provider_ops s_ops = {
  .set_performance_state = prv_set,
};

PBL_POWER_DOMAIN_PROVIDER_STATE_DEFINE(s_provider, 2);
static const struct pbl_power_domain_provider s_provider =
    PBL_POWER_DOMAIN_PROVIDER_INIT(s_provider, "pd", nullptr, nullptr, &s_ops);

static const struct pbl_power_domain s_pd1 = PBL_POWER_DOMAIN(&s_provider, 1);
static const struct pbl_power_domain s_pd_bad = PBL_POWER_DOMAIN(&s_provider, 2);

void test_power_domain__initialize(void) {
  s_calls = 0U;
  s_ret = 0;
  memset(s_provider_pd_levels, 0, sizeof(s_provider_pd_levels));
  cl_assert_equal_i(pbl_device_init(&s_provider.dev), 0);
}

void test_power_domain__set(void) {
  cl_assert_equal_i(s_provider.num_domains, 2);
  cl_assert_equal_i(pbl_power_domain_get_performance_state(&s_pd1), 0);

  cl_assert_equal_i(pbl_power_domain_set_performance_state(&s_pd1, 4), 0);
  cl_assert_equal_i(s_calls, 1);
  cl_assert_equal_i(s_last_id, 1);
  cl_assert_equal_i(s_last_level, 4);
  cl_assert_equal_i(pbl_power_domain_get_performance_state(&s_pd1), 4);

  cl_assert_equal_i(pbl_power_domain_set_performance_state(&s_pd1, 4), 0);
  cl_assert_equal_i(s_calls, 1);
}

void test_power_domain__failure_keeps_the_state(void) {
  cl_assert_equal_i(pbl_power_domain_set_performance_state(&s_pd1, 2), 0);
  s_ret = -EIO;
  cl_assert_equal_i(pbl_power_domain_set_performance_state(&s_pd1, 3), -EIO);
  cl_assert_equal_i(pbl_power_domain_get_performance_state(&s_pd1), 2);
}

void test_power_domain__invalid_domain(void) {
  cl_assert_equal_i(pbl_power_domain_set_performance_state(&s_pd_bad, 1), -EINVAL);
  cl_assert_equal_i(pbl_power_domain_get_performance_state(&s_pd_bad), 0);
  cl_assert_equal_i(s_calls, 0);
}
