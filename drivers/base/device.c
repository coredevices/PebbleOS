/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <stddef.h>

#include <pbl/device.h>
#include <pbl/logging/logging.h>

#include <system/passert.h>

PBL_LOG_MODULE_DEFINE(device, CONFIG_DEVICE_LOG_LEVEL);

#ifdef PBL_NO_LINKER_SCRIPT
extern const struct pbl_device *const __pbl_devices_start[] PBL_UNSORTED_SECTION_START(pbl_devices);
extern const struct pbl_device *const __pbl_devices_end[] PBL_UNSORTED_SECTION_END(pbl_devices);
#else
extern const struct pbl_device *const __pbl_devices_start[];
extern const struct pbl_device *const __pbl_devices_end[];
#endif

static int prv_init_deps(const struct pbl_device *dev) {
  const struct pbl_device *parent = dev->parent;

  // A parent bringing its children up from its own init is not waited for
  if (parent != NULL && parent->state->status != PBL_DEVICE_INITIALIZING &&
      pbl_device_init(parent) != 0) {
    PBL_LOG_DBG("%s: parent %s unavailable", dev->name, parent->name);
    return -ENODEV;
  }

  if (dev->deps == NULL) {
    return 0;
  }

  for (const struct pbl_device *const *dep = dev->deps; *dep != NULL; dep++) {
    if (pbl_device_init(*dep) != 0) {
      PBL_LOG_DBG("%s: dependency %s unavailable", dev->name, (*dep)->name);
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
      PBL_ASSERTN(dev->parent != NULL && dev->parent->state->status == PBL_DEVICE_INITIALIZING);
      return 0;
    default:
      break;
  }

  state->status = PBL_DEVICE_INITIALIZING;

  res = prv_init_deps(dev);
  if (res == 0 && dev->init != NULL) {
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

  for (const struct pbl_device *const *dev = __pbl_devices_start; dev < __pbl_devices_end; dev++) {
    if (pbl_device_init(*dev) != 0) {
      failures++;
    }
  }

  return failures;
}

int pbl_device_init_children(const struct pbl_device *parent) {
  int failures = 0;

  for (const struct pbl_device *const *dev = __pbl_devices_start; dev < __pbl_devices_end; dev++) {
    if ((*dev)->parent == parent && pbl_device_init(*dev) != 0) {
      failures++;
    }
  }

  return failures;
}
