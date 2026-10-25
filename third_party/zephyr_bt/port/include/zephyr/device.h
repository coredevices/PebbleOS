/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

// Minimal device model: the only device is the HCI driver bridging to the
// PebbleOS HCI router.

#pragma once

#include <stdbool.h>
#include <stddef.h>

struct device {
  const char *name;
  const void *api;
  void *data;
};

#define DEVICE_API_GET(_class, _dev) ((const struct _class##_driver_api *)(_dev)->api)
#define DEVICE_API_IS(_class, _dev)  true

static inline bool device_is_ready(const struct device *dev) {
  return dev != NULL;
}
