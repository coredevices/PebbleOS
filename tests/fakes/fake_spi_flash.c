/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "fake_spi_flash.h"

#include <pbl/drivers/flash.h>
#include "flash_region/flash_region.h"
#include "system/status_codes.h"

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "clar_asserts.h"

typedef struct FakeFlashState {
  uint32_t offset;
  uint32_t length;
  uint32_t bytes_left_till_write_failure;
  jmp_buf *jmp_on_failure;
  uint8_t* storage; //! Allocated buffer of length bytes.
  uint32_t write_count;
  uint32_t erase_count;
} FakeFlashState;

static FakeFlashState s_state = { 0 };

void fake_spi_flash_erase(void) {
  memset(s_state.storage, 0xff, s_state.length);
}

void fake_spi_flash_cleanup(void) {
  free(s_state.storage);
  s_state.storage = NULL;
  s_state = (FakeFlashState) { 0 };
}

//! @param offset the offset at which this fake region of flash begins.
//! @param length the length of this fake region of flash.
void fake_spi_flash_init(uint32_t offset, uint32_t length) {
  // Clients are not required to cleanup due to prior code, so do so here.
  fake_spi_flash_cleanup();

  // Tests historically pass (0, 0x1000000) — sized for snowy's flash layout where the filesystem
  // sits inside the first 16 MB. On platforms with a non-zero FLASH_REGION_BASE_ADDRESS (e.g.
  // obelix, whose flash addresses are absolute and start at 0x12000000), translate offset 0 to the
  // base address so the fake window doesn't span the huge unused gap below it, and then expand the
  // window if it still doesn't reach FLASH_REGION_FILESYSTEM_END. Without this, obelix tests would
  // allocate ~319 MB per test and trip DUMA's protect-free OOM guard.
#if defined(FLASH_REGION_BASE_ADDRESS) && (FLASH_REGION_BASE_ADDRESS > 0)
  if (offset == 0) {
    offset = FLASH_REGION_BASE_ADDRESS;
  }
#endif
#ifdef FLASH_REGION_FILESYSTEM_END
  if (offset + length < FLASH_REGION_FILESYSTEM_END) {
    length = FLASH_REGION_FILESYSTEM_END - offset;
  }
#endif
#ifdef FLASH_REGION_SHARED_PRF_STORAGE_END
  // On platforms with regions beyond the filesystem (TZINFO, MFG_INFO,
  // SHARED_PRF_STORAGE — present on gd25q256e/obelix layouts), grow far enough
  // to cover them so flash_read_bytes doesn't trip the bounds check.
  if (offset + length < FLASH_REGION_SHARED_PRF_STORAGE_END) {
    length = FLASH_REGION_SHARED_PRF_STORAGE_END - offset;
  }
#endif

  s_state.offset = offset;
  s_state.length = length;
  s_state.storage = malloc(length);
  s_state.write_count = 0;
  // Note: this is a harness failure, not a code failure.
  cl_assert(s_state.storage != NULL);
  memset(s_state.storage, 0xff, length);
}

void fake_flash_assert_region_untouched(uint32_t start_addr, uint32_t length) {
  if (length == 0) {
    return;
  }

  for (uint32_t i = 0; i < length; i++) {
    cl_assert(s_state.storage[start_addr + i] == 0xff);
  }
}

int32_t fake_spi_flash_find_next_write(int32_t offset) {
  if(offset < s_state.offset || offset >= s_state.offset + s_state.length) {
    return E_RANGE;
  }
  do {
    if(s_state.storage[offset] != 0xff) {
      return offset;
    }
    offset++;
  } while(offset < s_state.offset + s_state.length);
  return E_DOES_NOT_EXIST;
}

void fake_spi_flash_populate_from_file(char *path, uint32_t offset) {
  cl_assert(s_state.storage);
  cl_assert(offset >= s_state.offset);
  cl_assert((offset - s_state.offset) <= s_state.length);

  // find the offset in the storage array
  uint32_t fake_offset = offset - s_state.offset;

  // check that file exists and fits in buffer
  struct stat st;
  cl_assert(stat(path, &st) == 0);
  cl_assert(st.st_size < (s_state.length - fake_offset));

  FILE *file = fopen(path, "r");
  cl_assert(file);

  // copy file to fake flash storage
  cl_assert(fread(&s_state.storage[fake_offset], 1, st.st_size, file) > 0);
}

void fake_spi_flash_force_future_failure(int after_n_bytes, jmp_buf *retire_to) {
  s_state.bytes_left_till_write_failure = after_n_bytes;
  s_state.jmp_on_failure = retire_to;
}

static void prv_check_range(uint32_t addr, size_t len) {
  cl_assert(addr >= s_state.offset);
  cl_assert(addr + len <= s_state.offset + s_state.length);
}

int pbl_flash_read(const struct pbl_flash_device *dev, uint32_t addr, void *buf, size_t len) {
  prv_check_range(addr, len);
  memcpy(buf, s_state.storage + (addr - s_state.offset), len);
  return 0;
}

int pbl_flash_write(const struct pbl_flash_device *dev, uint32_t addr, const void *buf,
                    size_t len) {
  const uint8_t *src = buf;

  prv_check_range(addr, len);
  ++s_state.write_count;

  for (size_t i = 0; i < len; ++i) {
    if (s_state.jmp_on_failure != NULL) {
      if (s_state.bytes_left_till_write_failure == 0) {
        longjmp(*s_state.jmp_on_failure, 1);
      } else {
        s_state.bytes_left_till_write_failure--;
      }
    }
    // Only ones can be turned into zeros
    s_state.storage[addr - s_state.offset + i] &= src[i];
  }

  return 0;
}

static void prv_erase_block(uint32_t addr, uint32_t size) {
  ++s_state.erase_count;
  prv_check_range(addr, size);
  memset(&s_state.storage[addr - s_state.offset], 0xff, size);
}

int pbl_flash_erase(const struct pbl_flash_device *dev, uint32_t addr, size_t len) {
  cl_assert((addr & (SUBSECTOR_SIZE_BYTES - 1)) == 0);
  len = (len + SUBSECTOR_SIZE_BYTES - 1) & SUBSECTOR_ADDR_MASK;

  while (len > 0) {
    uint32_t unit = SUBSECTOR_SIZE_BYTES;
    if ((addr & (SECTOR_SIZE_BYTES - 1)) == 0 && len >= SECTOR_SIZE_BYTES) {
      unit = SECTOR_SIZE_BYTES;
    }
    prv_erase_block(addr, unit);
    addr += unit;
    len -= unit;
  }

  return 0;
}

int pbl_flash_erase_async(const struct pbl_flash_device *dev, uint32_t addr, size_t len,
                          pbl_flash_erase_cb_t cb, void *ctx) {
  pbl_flash_erase(dev, addr, len);
  cb(ctx, 0);
  return 0;
}

bool pbl_flash_is_erased(const struct pbl_flash_device *dev, uint32_t addr, size_t len) {
  prv_check_range(addr, len);
  for (size_t i = 0; i < len; i++) {
    if (s_state.storage[addr - s_state.offset + i] != 0xff) {
      return false;
    }
  }
  return true;
}

int pbl_flash_protect(const struct pbl_flash_device *dev, uint32_t addr, size_t len) {
  return 0;
}

int pbl_flash_unprotect(const struct pbl_flash_device *dev) { return 0; }

int pbl_flash_init(const struct pbl_flash_device *dev) { return 0; }

void pbl_flash_stop(const struct pbl_flash_device *dev) {}

static const struct pbl_flash_geometry s_fake_geometry = {
    .size = UINT32_MAX,
    .page_size = 256,
    .sector_size = SECTOR_SIZE_BYTES,
    .subsector_size = SUBSECTOR_SIZE_BYTES,
};
static struct pbl_flash_device_state s_fake_device_state;
static const struct pbl_flash_device s_fake_device = {
    .state = &s_fake_device_state,
    .base = 0,
    .geometry = &s_fake_geometry,
};
const struct pbl_flash_device *const FLASH = &s_fake_device;

uint32_t fake_flash_write_count(void) {
  return s_state.write_count;
}

uint32_t fake_flash_erase_count(void) {
  return s_state.erase_count;
}
