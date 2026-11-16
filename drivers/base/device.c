/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <stddef.h>
#include <string.h>

#include <pbl/device.h>
#include <pbl/kernel/assert.h>
#include <pbl/logging/logging.h>

PBL_LOG_MODULE_DEFINE(device, CONFIG_DEVICE_LOG_LEVEL);

PBL_ITERABLE_SECTION_DECLARE(const struct pbl_device *const, devices);
PBL_ITERABLE_SECTION_DECLARE(const struct pbl_device_depends, dev_deps);

static int prv_init_list(const struct pbl_device *dev, const struct pbl_device *const *deps) {
  for (const struct pbl_device *const *dep = deps; *dep != nullptr; dep++) {
    if (pbl_device_init(*dep) != 0) {
      PBL_LOG_DBG("%s: dependency %s unavailable", dev->name, (*dep)->name);
      return -ENODEV;
    }
  }

  return 0;
}

static int prv_init_deps(const struct pbl_device *dev) {
  const struct pbl_device *parent = dev->parent;

  // A parent bringing its children up from its own init is not waited for
  if (parent != nullptr && parent->state->status != PBL_DEVICE_INITIALIZING &&
      pbl_device_init(parent) != 0) {
    PBL_LOG_DBG("%s: parent %s unavailable", dev->name, parent->name);
    return -ENODEV;
  }

  if (dev->deps != nullptr && prv_init_list(dev, dev->deps) != 0) {
    return -ENODEV;
  }

  PBL_ITERABLE_SECTION_FOREACH(const struct pbl_device_depends, dev_deps, d) {
    if (d->dev == dev && prv_init_list(dev, d->deps) != 0) {
      return -ENODEV;
    }
  }

  return 0;
}

int pbl_device_init(const struct pbl_device *dev) {
  struct pbl_device_state *state = dev->state;
  int res;

  switch (state->status) {
    case PBL_DEVICE_READY:
    case PBL_DEVICE_FAILED:
      return state->res;
    case PBL_DEVICE_INITIALIZING:
      // Only a parent may re-enter a child on its way up; anything else is a cycle
      PBL_ASSERTN(dev->parent != nullptr && dev->parent->state->status == PBL_DEVICE_INITIALIZING);
      return 0;
    default:
      break;
  }

  state->status = PBL_DEVICE_INITIALIZING;

  res = prv_init_deps(dev);
  if (res == 0 && dev->init != nullptr) {
    res = dev->init(dev);
    if (res == -ENODEV) {
      PBL_LOG_DBG("%s not present", dev->name);
    } else if (res != 0) {
      PBL_LOG_ERR("%s init failed (%d)", dev->name, res);
    }
  }

  state->res = res;
  state->status = (res == 0) ? PBL_DEVICE_READY : PBL_DEVICE_FAILED;

  return res;
}

bool pbl_device_is_ready(const struct pbl_device *dev) {
  return dev->state->status == PBL_DEVICE_READY;
}

int pbl_device_init_all(void) {
  int failures = 0;

#ifdef CONFIG_DEVICE_INIT_REVERSE
  for (const struct pbl_device *const *dev = PBL_ITERABLE_SECTION_END(devices);
       dev-- > PBL_ITERABLE_SECTION_START(devices);) {
#else
  PBL_ITERABLE_SECTION_FOREACH(const struct pbl_device *const, devices, dev) {
#endif
    if (pbl_device_init(*dev) != 0) {
      failures++;
    }
  }

  return failures;
}

int pbl_device_init_children(const struct pbl_device *parent) {
  int failures = 0;

  PBL_ITERABLE_SECTION_FOREACH(const struct pbl_device *const, devices, dev) {
    if ((*dev)->parent == parent && pbl_device_init(*dev) != 0) {
      failures++;
    }
  }

  return failures;
}

const struct pbl_device *pbl_device_class_next(const struct pbl_device_class *cls,
                                               const struct pbl_device *prev) {
  const struct pbl_device *const *dev = PBL_ITERABLE_SECTION_START(devices);

  if (prev != nullptr) {
    while (dev < PBL_ITERABLE_SECTION_END(devices) && *dev != prev) {
      dev++;
    }
    dev++;
  }

  for (; dev < PBL_ITERABLE_SECTION_END(devices); dev++) {
    if ((*dev)->cls == cls) {
      return *dev;
    }
  }

  return nullptr;
}

const struct pbl_device *pbl_device_find(const struct pbl_device_class *cls, const char *name) {
  const struct pbl_device *dev;

  PBL_DEVICE_CLASS_FOREACH(cls, dev) {
    if (strcmp(dev->name, name) == 0) {
      return dev;
    }
  }

  return nullptr;
}
