/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <string.h>

#include <pbl/device.h>
#include <pbl/util/misc.h>
#include <pbl/util/size.h>

#include <clar.h>
#include <stubs_logging.h>
#include <stubs_passert.h>

struct test_dev {
  struct pbl_device dev;
  int res;
  char tag;
};

static char s_order[16];

static int prv_init(const struct pbl_device *dev) {
  const struct test_dev *td = container_of(dev, const struct test_dev, dev);
  size_t n = strlen(s_order);

  s_order[n] = td->tag;
  s_order[n + 1] = '\0';

  return td->res;
}

#define TEST_DEV(sym, _tag, _init, _parent, _deps)            \
  PBL_DEVICE_STATE_DEFINE(sym);                               \
  static struct test_dev sym = {                              \
    .dev = PBL_DEVICE_INIT(sym, #sym, _init, _parent, _deps), \
    .tag = _tag,                                              \
  }

#define TEST_DEV_REGISTERED(sym, _tag, _init, _parent, _deps) \
  TEST_DEV(sym, _tag, _init, _parent, _deps);                 \
  PBL_DEVICE_REGISTER(sym, &sym.dev)

// gpio <- i2c <- pmic <- mfd (MFD bringing its child up), sensor on i2c using gpio, absent on i2c
static struct test_dev s_gpio, s_i2c, s_pmic, s_mfd;

static int prv_mfd_init(const struct pbl_device *dev) {
  int res = prv_init(dev);

  return (res != 0) ? res : pbl_device_init_children(dev);
}

TEST_DEV_REGISTERED(s_sensor, 's', prv_init, &s_i2c.dev, PBL_DEVICE_DEPS(&s_gpio.dev));
TEST_DEV_REGISTERED(s_gpio, 'g', prv_init, NULL, NULL);
TEST_DEV_REGISTERED(s_pmic, 'p', prv_init, &s_i2c.dev, NULL);
TEST_DEV_REGISTERED(s_i2c, 'i', prv_init, NULL, PBL_DEVICE_DEPS(&s_gpio.dev));
TEST_DEV_REGISTERED(s_mfd_child, 'c', prv_init, &s_mfd.dev, NULL);
TEST_DEV_REGISTERED(s_mfd, 'm', prv_mfd_init, &s_pmic.dev, NULL);
TEST_DEV_REGISTERED(s_absent, 'a', prv_init, &s_i2c.dev, NULL);
TEST_DEV_REGISTERED(s_noinit, 'n', NULL, NULL, NULL);

// a <-> b, not registered
TEST_DEV(s_cycle_a, 'x', prv_init, NULL, NULL);
TEST_DEV(s_cycle_b, 'y', prv_init, NULL, PBL_DEVICE_DEPS(&s_cycle_a.dev));

static struct test_dev *const s_all[] = {
  &s_sensor, &s_gpio,   &s_pmic,   &s_i2c,     &s_mfd_child,
  &s_mfd,    &s_absent, &s_noinit, &s_cycle_a, &s_cycle_b,
};

void test_device__initialize(void) {
  s_order[0] = '\0';
  for (size_t i = 0; i < ARRAY_LENGTH(s_all); i++) {
    *s_all[i]->dev.state = (struct pbl_device_state){0};
    s_all[i]->res = 0;
  }
  s_absent.res = -ENODEV;
}

void test_device__deps_first(void) {
  cl_assert_equal_i(pbl_device_init(&s_pmic.dev), 0);
  cl_assert_equal_s(s_order, "gip");
  cl_assert(pbl_device_is_ready(&s_gpio.dev));
  cl_assert(pbl_device_is_ready(&s_i2c.dev));
  cl_assert(pbl_device_is_ready(&s_pmic.dev));
  cl_assert(!pbl_device_is_ready(&s_sensor.dev));
}

void test_device__init_is_idempotent(void) {
  cl_assert_equal_i(pbl_device_init(&s_i2c.dev), 0);
  cl_assert_equal_i(pbl_device_init(&s_i2c.dev), 0);
  cl_assert_equal_i(pbl_device_init(&s_gpio.dev), 0);
  cl_assert_equal_s(s_order, "gi");
}

void test_device__init_all(void) {
  cl_assert_equal_i(pbl_device_init_all(), 1);
  cl_assert_equal_i(strlen(s_order), 7);
  cl_assert(strchr(s_order, 'g') < strchr(s_order, 'i'));
  cl_assert(strchr(s_order, 'i') < strchr(s_order, 's'));
  cl_assert(strchr(s_order, 'i') < strchr(s_order, 'p'));
  cl_assert(strchr(s_order, 'p') < strchr(s_order, 'm'));
  cl_assert(strchr(s_order, 'm') < strchr(s_order, 'c'));
  cl_assert(strchr(s_order, 'a') != NULL);
  cl_assert(pbl_device_is_ready(&s_noinit.dev));
  cl_assert(!pbl_device_is_ready(&s_absent.dev));
  cl_assert(!pbl_device_is_ready(&s_cycle_a.dev));
}

void test_device__parent_inits_children(void) {
  cl_assert_equal_i(pbl_device_init(&s_mfd.dev), 0);
  cl_assert_equal_s(s_order, "gipmc");
  cl_assert(pbl_device_is_ready(&s_mfd_child.dev));
}

void test_device__child_pulls_parent(void) {
  cl_assert_equal_i(pbl_device_init(&s_mfd_child.dev), 0);
  cl_assert_equal_s(s_order, "gipmc");
  cl_assert(pbl_device_is_ready(&s_mfd.dev));
}

void test_device__failed_parent_fails_children(void) {
  s_pmic.res = -EIO;
  cl_assert_equal_i(pbl_device_init(&s_mfd_child.dev), -ENODEV);
  cl_assert_equal_i(pbl_device_init(&s_mfd.dev), -ENODEV);
  cl_assert_equal_i(pbl_device_init(&s_pmic.dev), -EIO);
  cl_assert_equal_s(s_order, "gip");
}

void test_device__failed_dep_fails_dependents(void) {
  s_gpio.res = -EIO;
  cl_assert_equal_i(pbl_device_init(&s_sensor.dev), -ENODEV);
  cl_assert_equal_i(pbl_device_init(&s_i2c.dev), -ENODEV);
  cl_assert_equal_s(s_order, "g");
}

void test_device__absent(void) {
  cl_assert_equal_i(pbl_device_init(&s_absent.dev), -ENODEV);
  cl_assert_equal_s(s_order, "gia");
  cl_assert(!pbl_device_is_ready(&s_absent.dev));
  cl_assert(pbl_device_is_ready(&s_i2c.dev));
}

void test_device__cycle_asserts(void) {
  s_cycle_a.dev.deps = PBL_DEVICE_DEPS(&s_cycle_b.dev);
  cl_assert_passert(pbl_device_init(&s_cycle_a.dev));
  s_cycle_a.dev.deps = NULL;
}
