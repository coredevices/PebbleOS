/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/nvmem.h>

PBL_DEVICE_CLASS_DEFINE(pbl_nvmem_class, "nvmem");

int pbl_nvmem_device_read(const struct pbl_nvmem *nvmem, unsigned int offset, size_t bytes,
                          void *buf) {
  if (!pbl_device_is_ready(&nvmem->dev)) {
    return -ENODEV;
  }

  if (bytes == 0U || offset >= nvmem->size || bytes > nvmem->size - offset ||
      offset % nvmem->stride != 0U || bytes % nvmem->word_size != 0U) {
    return -EINVAL;
  }

  return nvmem->ops->reg_read(nvmem, offset, buf, bytes);
}

int pbl_nvmem_cell_read(const struct pbl_nvmem_cell *cell, void *buf, size_t len) {
  if (len < cell->bytes) {
    return -EINVAL;
  }

  return pbl_nvmem_device_read(cell->nvmem, cell->offset, cell->bytes, buf);
}
