/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <string.h>

#include <pbl/loader/loader.h>
#include <pbl/logging/logging.h>
#include <pbl/util/math.h>

PBL_LOG_MODULE_DEFINE(loader, CONFIG_LOADER_LOG_LEVEL);

#define RELOC_ENTRY_SIZE sizeof(uint32_t)

static bool prv_range_fits(size_t offset, size_t size, size_t limit) {
  return (offset <= limit) && (size <= (limit - offset));
}

static int prv_check(const struct pbl_loader_image *img, size_t dst_size) {
  if ((img->header_size > img->file_size) || (img->file_size > img->mem_size)) {
    PBL_LOG_WRN("Invalid image layout: header=%zu file=%zu mem=%zu", img->header_size,
                img->file_size, img->mem_size);
    return -EINVAL;
  }

  if (img->mem_size > dst_size) {
    PBL_LOG_WRN("Image does not fit: mem=%zu dst=%zu", img->mem_size, dst_size);
    return -ENOMEM;
  }

  return 0;
}

int pbl_loader_copy(const struct pbl_loader_image *img, const struct pbl_loader_source *src,
                    void *dst, size_t dst_size) {
  int ret = prv_check(img, dst_size);
  if (ret < 0) {
    return ret;
  }

  ret = src->read(src->ctx, 0, dst, img->file_size);
  if (ret < 0) {
    PBL_LOG_WRN("Image read failed: %d", ret);
    return ret;
  }

  memset((uint8_t *)dst + img->file_size, 0, img->mem_size - img->file_size);

  return 0;
}

static int prv_apply(const struct pbl_loader_image *img, uint8_t *dst, const uint8_t *table,
                     pbl_loader_address_t address, void *ctx) {
  for (size_t i = 0; i < img->reloc_count; ++i) {
    uint32_t target;
    memcpy(&target, &table[i * RELOC_ENTRY_SIZE], sizeof(target));
    if (!prv_range_fits(target, sizeof(uint32_t), img->file_size) || (target < img->header_size)) {
      PBL_LOG_WRN("Invalid relocation target[%zu]: 0x%" PRIx32, i, target);
      return -EINVAL;
    }

    uint32_t value;
    memcpy(&value, &dst[target], sizeof(value));
    // One past the end of the image is a valid pointer, it is just never dereferenced
    if (value > img->mem_size) {
      PBL_LOG_WRN("Invalid relocation value[%zu]: 0x%" PRIx32, i, value);
      return -EINVAL;
    }

    const uint32_t relocated =
        address ? address(ctx, &dst[value]) : (uint32_t)(uintptr_t)&dst[value];
    memcpy(&dst[target], &relocated, sizeof(relocated));
  }

  return 0;
}

static int prv_read_and_apply(const struct pbl_loader_image *img,
                              const struct pbl_loader_source *src, uint8_t *dst, uint8_t *table,
                              size_t table_size, pbl_loader_address_t address, void *ctx) {
  int ret = src->read(src->ctx, img->reloc_offset, table, table_size);
  if (ret < 0) {
    PBL_LOG_WRN("Relocation table read failed: %d", ret);
    return ret;
  }

  return prv_apply(img, dst, table, address, ctx);
}

int pbl_loader_relocate(const struct pbl_loader_image *img, const struct pbl_loader_source *src,
                        void *dst, size_t dst_size, pbl_loader_address_t address, void *ctx) {
  int ret = prv_check(img, dst_size);
  if (ret < 0) {
    return ret;
  }

  if (img->reloc_count == 0) {
    return 0;
  }

  size_t table_size;
  if (pbl_size_mul_overflow(img->reloc_count, RELOC_ENTRY_SIZE, &table_size) ||
      !prv_range_fits(img->file_size, table_size, dst_size)) {
    PBL_LOG_WRN("Relocation table does not fit: entries=%zu dst=%zu", img->reloc_count, dst_size);
    return -ENOMEM;
  }

  uint8_t *table = (uint8_t *)dst + img->file_size;
  ret = prv_read_and_apply(img, src, dst, table, table_size, address, ctx);
  memset(table, 0, table_size);

  return ret;
}
