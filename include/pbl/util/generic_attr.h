/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "pbl/kernel/compiler.h"

#include <stdint.h>
#include <stddef.h>

struct PBL_PACKED pbl_generic_attr {
  uint8_t id;
  uint16_t length;
  uint8_t data[];
};

struct PBL_PACKED pbl_generic_attr_list {
  uint8_t num_attributes;
  struct pbl_generic_attr attributes[];
};

struct pbl_generic_attr *pbl_generic_attr_find(struct pbl_generic_attr_list *attr_list, uint8_t id,
                                               size_t size);

struct pbl_generic_attr *pbl_generic_attr_add(struct pbl_generic_attr *attr, uint8_t id,
                                              const void *data, size_t size);
