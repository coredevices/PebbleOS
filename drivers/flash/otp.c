/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "flash_priv.h"

#include <errno.h>
#include <string.h>

#include <pbl/drivers/flash.h>
#include <pbl/util/math.h>

#define OTP_RESUME_AFTER_READ_MS  5U
#define OTP_RESUME_AFTER_WRITE_MS 50U

static bool prv_has_otp(const struct pbl_flash *flash) {
  return flash->otp != nullptr && flash->otp->n_regions > 0U;
}

static int prv_check_range(const struct pbl_flash *flash, uint32_t offs, size_t len) {
  const uint32_t size = flash->otp->region_len * flash->otp->n_regions;

  if (len > size || offs > (size - len)) {
    return -EINVAL;
  }

  return 0;
}

static int prv_check_region(const struct pbl_flash *flash, uint8_t region) {
  if (!prv_has_otp(flash)) {
    return -ENOTSUP;
  }

  return (region < flash->otp->n_regions) ? 0 : -EINVAL;
}

int pbl_flash_otp_get_info(const struct pbl_flash *flash, struct pbl_flash_otp_info *info) {
  if (!prv_has_otp(flash)) {
    return -ENOTSUP;
  }

  *info = *flash->otp;

  return 0;
}

int pbl_flash_otp_read(const struct pbl_flash *flash, uint32_t offs, void *buf, size_t len) {
  uint8_t *dst = buf;
  uint32_t region_len;
  int ret;

  if (!prv_has_otp(flash)) {
    return -ENOTSUP;
  }

  region_len = flash->otp->region_len;

  ret = prv_check_range(flash, offs, len);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_flash__access_begin(flash, OTP_RESUME_AFTER_READ_MS);
  if (ret != 0) {
    return ret;
  }

  while (len > 0U && ret == 0) {
    uint32_t roffs = offs % region_len;
    size_t chunk = MIN(len, region_len - roffs);

    ret = flash->ops->otp_read(flash, (uint8_t)(offs / region_len), roffs, dst, chunk);
    offs += chunk;
    dst += chunk;
    len -= chunk;
  }

  pbl_flash__access_end(flash);

  return ret;
}

int pbl_flash_otp_write(const struct pbl_flash *flash, uint32_t offs, const void *buf, size_t len) {
  const uint8_t *src = buf;
  uint32_t region_len;
  bool locked;
  int ret;

  if (!prv_has_otp(flash)) {
    return -ENOTSUP;
  }

  region_len = flash->otp->region_len;

  ret = prv_check_range(flash, offs, len);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_flash__access_begin(flash, OTP_RESUME_AFTER_WRITE_MS);
  if (ret != 0) {
    return ret;
  }

  while (len > 0U) {
    uint8_t region = (uint8_t)(offs / region_len);
    uint32_t roffs = offs % region_len;
    size_t chunk = MIN(len, region_len - roffs);

    ret = flash->ops->otp_is_locked(flash, region, &locked);
    if (ret == 0 && locked) {
      ret = -EROFS;
    }
    if (ret != 0) {
      break;
    }

    ret = flash->ops->otp_write(flash, region, roffs, src, chunk);
    if (ret != 0) {
      break;
    }

    offs += chunk;
    src += chunk;
    len -= chunk;
  }

  pbl_flash__access_end(flash);

  return ret;
}

int pbl_flash_otp_is_locked(const struct pbl_flash *flash, uint8_t region, bool *locked) {
  int ret;

  ret = prv_check_region(flash, region);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_flash__access_begin(flash, OTP_RESUME_AFTER_READ_MS);
  if (ret != 0) {
    return ret;
  }

  ret = flash->ops->otp_is_locked(flash, region, locked);
  pbl_flash__access_end(flash);

  return ret;
}

int pbl_flash_otp_lock(const struct pbl_flash *flash, uint8_t region) {
  int ret;

  ret = prv_check_region(flash, region);
  if (ret != 0) {
    return ret;
  }

  if (flash->ops->otp_lock == nullptr) {
    return -ENOTSUP;
  }

  ret = pbl_flash__access_begin(flash, OTP_RESUME_AFTER_WRITE_MS);
  if (ret != 0) {
    return ret;
  }

  ret = flash->ops->otp_lock(flash, region);
  pbl_flash__access_end(flash);

  return ret;
}

int pbl_flash_otp_erase(const struct pbl_flash *flash, uint8_t region) {
  bool locked;
  int ret;

  ret = prv_check_region(flash, region);
  if (ret != 0) {
    return ret;
  }

  if (flash->ops->otp_erase == nullptr) {
    return -ENOTSUP;
  }

  ret = pbl_flash__access_begin(flash, OTP_RESUME_AFTER_WRITE_MS);
  if (ret != 0) {
    return ret;
  }

  ret = flash->ops->otp_is_locked(flash, region, &locked);
  if (ret == 0 && locked) {
    ret = -EROFS;
  }
  if (ret == 0) {
    ret = flash->ops->otp_erase(flash, region);
  }

  pbl_flash__access_end(flash);

  return ret;
}

const struct pbl_flash_otp_cell *pbl_flash_otp_cell_get(const struct pbl_flash *flash,
                                                        const char *name) {
  const struct pbl_flash_otp_layout *layout = flash->otp_layout;

  if (layout == nullptr) {
    return nullptr;
  }

  for (uint8_t i = 0U; i < layout->num_cells; i++) {
    if (strcmp(layout->cells[i].name, name) == 0) {
      return &layout->cells[i];
    }
  }

  return nullptr;
}
