/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "../coredump_priv.h"

#include <errno.h>
#include <stdint.h>

#include <pbl/board/partitions.h>
#include <pbl/coredump/backend.h>
#include <pbl/coredump/format.h>
#include <pbl/drivers/flash.h>
#include <pbl/drivers/flash/partition.h>

#include <system/firmware_storage.h>

#ifdef CONFIG_PBLBOOT
#define STORAGE_START PART_CD_OFFSET
#define STORAGE_SIZE  PART_CD_SIZE
#else
// Shares the only firmware slot
#define STORAGE_START PART_FIRMWARE_DEST_OFFSET
#define STORAGE_SIZE  PART_FIRMWARE_DEST_SIZE
#endif

#define IMAGES CONFIG_COREDUMP_BACKEND_FLASH_IMAGES

#define SLOT_SIZE (((STORAGE_SIZE - SUBSECTOR_SIZE_BYTES) / IMAGES) & SUBSECTOR_ADDR_MASK)

#define FLASH_HDR_MAGIC 0x464C5300
#define ALL_UNFORMATTED UINT32_MAX
#define MIN_AGE_SECONDS (60 * 60 * 24)

// At the start of the storage, followed a subsector later by the slots.
struct pbl_coredump__flash_hdr {
  uint32_t magic;
  // Bit n set means slot n was never used.
  uint32_t unformatted;
};

// At the start of each slot, followed by the image.
struct pbl_coredump__slot_hdr {
  uint32_t magic;
  // The slot with the highest value is the most recent one. Always >= 1.
  uint32_t last_used;
  // Non-zero if the image has not been read back.
  uint8_t unread;
};

static uint32_t s_addr;
static uint32_t s_slot_end;

static uint32_t prv_slot_addr(uint32_t slot) {
  return STORAGE_START + SUBSECTOR_SIZE_BYTES + slot * SLOT_SIZE;
}

static uint32_t prv_image_addr(uint32_t slot) {
  return prv_slot_addr(slot) + sizeof(struct pbl_coredump__slot_hdr);
}

static int prv_latest_slot(const struct pbl_coredump__flash_hdr *flash_hdr, bool unread_only,
                           uint32_t *slot, uint32_t *last_used) {
  *last_used = 0;

  for (uint32_t i = 0; i < IMAGES; i++) {
    struct pbl_coredump__slot_hdr hdr;

    if (flash_hdr->unformatted & (1U << i)) {
      continue;
    }

    int ret = pbl_flash_read(FLASH, prv_slot_addr(i), &hdr, sizeof(hdr));
    if (ret < 0) {
      return ret;
    }

    if (hdr.magic == FLASH_HDR_MAGIC && hdr.last_used > *last_used &&
        (!unread_only || hdr.unread != 0)) {
      *last_used = hdr.last_used;
      *slot = i;
    }
  }

  return (*last_used > 0) ? 0 : -ENOENT;
}

static int prv_keep_stored_image(uint32_t slot, uint32_t timestamp) {
  struct pbl_coredump__slot_hdr slot_hdr;
  struct pbl_coredump_image_hdr image_hdr;

  int ret = pbl_flash_read(FLASH, prv_slot_addr(slot), &slot_hdr, sizeof(slot_hdr));
  if (ret < 0) {
    return ret;
  }

  ret = pbl_flash_read(FLASH, prv_image_addr(slot), &image_hdr, sizeof(image_hdr));
  if (ret < 0) {
    return ret;
  }

  if (image_hdr.magic == PBL_COREDUMP_MAGIC && slot_hdr.unread != 0 &&
      (timestamp - image_hdr.time_stamp) < MIN_AGE_SECONDS) {
    return -EEXIST;
  }

  return 0;
}

static int prv_init(const struct pbl_coredump_backend *backend) {
  int ret = pbl_flash_coredump_init(FLASH);
  if (ret < 0) {
    return ret;
  }
  (void)pbl_flash_partition_protect(PART_SAFE_FIRMWARE);

  return 0;
}

static int prv_start(const struct pbl_coredump_backend *backend, bool forced, uint32_t timestamp) {
  struct pbl_coredump__flash_hdr flash_hdr;
  uint32_t slot = 0;
  uint32_t last_used;

  int ret = pbl_flash_read(FLASH, STORAGE_START, &flash_hdr, sizeof(flash_hdr));
  if (ret < 0) {
    return ret;
  }

  if (flash_hdr.magic != FLASH_HDR_MAGIC) {
    flash_hdr = (struct pbl_coredump__flash_hdr){
      .magic = FLASH_HDR_MAGIC,
      .unformatted = ALL_UNFORMATTED,
    };
    ret = pbl_flash_erase(FLASH, STORAGE_START, SUBSECTOR_SIZE_BYTES);
    if (ret < 0) {
      return ret;
    }
    ret = pbl_flash_write(FLASH, STORAGE_START, &flash_hdr, sizeof(flash_hdr));
    if (ret < 0) {
      return ret;
    }
  }

  ret = prv_latest_slot(&flash_hdr, false, &slot, &last_used);
  if (ret == 0) {
    if (!forced) {
      ret = prv_keep_stored_image(slot, timestamp);
      if (ret < 0) {
        return ret;
      }
    }
    slot = (slot + 1) % IMAGES;
  } else if (ret == -ENOENT) {
    // Nothing to rotate from: spread wear by starting anywhere.
    slot = timestamp % IMAGES;
  } else {
    return ret;
  }

  const struct pbl_coredump__slot_hdr slot_hdr = {
    .magic = FLASH_HDR_MAGIC,
    .last_used = last_used + 1,
    .unread = true,
  };
  const uint32_t addr = prv_slot_addr(slot);

  pbl_coredump__log_int("CD: saving to 0x", addr, 16);

  ret = pbl_flash_erase(FLASH, addr, SLOT_SIZE);
  if (ret < 0) {
    return ret;
  }
  ret = pbl_flash_write(FLASH, addr, &slot_hdr, sizeof(slot_hdr));
  if (ret < 0) {
    return ret;
  }

  flash_hdr.unformatted &= ~(1U << slot);
  ret = pbl_flash_write(FLASH, STORAGE_START, &flash_hdr, sizeof(flash_hdr));
  if (ret < 0) {
    return ret;
  }

  s_addr = prv_image_addr(slot);
  s_slot_end = addr + SLOT_SIZE;

  return 0;
}

static int prv_write(const struct pbl_coredump_backend *backend, const void *data, size_t len) {
  if (len > s_slot_end - s_addr) {
    return -ENOSPC;
  }

  int ret = pbl_flash_write(FLASH, s_addr, data, len);
  if (ret < 0) {
    return ret;
  }
  s_addr += len;

  return 0;
}

static size_t prv_space(const struct pbl_coredump_backend *backend) {
  return s_slot_end - s_addr;
}

static int prv_stored_find(const struct pbl_coredump_backend *backend, bool unread_only,
                           uint32_t *id) {
  struct pbl_coredump__flash_hdr flash_hdr;
  uint32_t last_used;

  int ret = pbl_flash_read(FLASH, STORAGE_START, &flash_hdr, sizeof(flash_hdr));
  if (ret < 0) {
    return ret;
  }
  if (flash_hdr.magic != FLASH_HDR_MAGIC) {
    return -ENOENT;
  }

  return prv_latest_slot(&flash_hdr, unread_only, id, &last_used);
}

static int prv_stored_read(const struct pbl_coredump_backend *backend, uint32_t id, uint32_t offset,
                           void *buf, size_t len) {
  const uint32_t size = SLOT_SIZE - sizeof(struct pbl_coredump__slot_hdr);

  if (id >= IMAGES || offset > size || len > size - offset) {
    return -EINVAL;
  }

  return pbl_flash_read(FLASH, prv_image_addr(id) + offset, buf, len);
}

static int prv_stored_mark_read(const struct pbl_coredump_backend *backend, uint32_t id) {
  struct pbl_coredump__slot_hdr hdr;

  if (id >= IMAGES) {
    return -EINVAL;
  }

  int ret = pbl_flash_read(FLASH, prv_slot_addr(id), &hdr, sizeof(hdr));
  if (ret < 0) {
    return ret;
  }
  hdr.unread = 0;

  return pbl_flash_write(FLASH, prv_slot_addr(id), &hdr, sizeof(hdr));
}

static const struct pbl_coredump_backend_api s_api = {
  .init = prv_init,
  .start = prv_start,
  .write = prv_write,
  .space = prv_space,
  .stored_find = prv_stored_find,
  .stored_read = prv_stored_read,
  .stored_mark_read = prv_stored_mark_read,
};

PBL_COREDUMP_BACKEND_DEFINE(flash, s_api);
