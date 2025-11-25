/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "fake_spi_flash.h"

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

void flash_read_bytes(uint8_t* buffer, uint32_t start_addr, uint32_t buffer_size) {
  cl_assert(start_addr >= s_state.offset);
  cl_assert(start_addr + buffer_size <= s_state.offset + s_state.length);

  memcpy(buffer, s_state.storage + (start_addr - s_state.offset), buffer_size);
}

void flash_write_bytes(const uint8_t* buffer, uint32_t start_addr, uint32_t buffer_size) {
  cl_assert(start_addr >= s_state.offset);
  cl_assert(start_addr + buffer_size <= s_state.offset + s_state.length);

  ++s_state.write_count;

  for (int i = 0; i < buffer_size; ++i) {
    if (s_state.jmp_on_failure != NULL) {
      if (s_state.bytes_left_till_write_failure == 0) {
        longjmp(*s_state.jmp_on_failure, 1);
      } else {
        s_state.bytes_left_till_write_failure--;
      }
    }
    // 0 write 0 = 0
    // 1 write 0 = 0
    // 1 write 1 = 1
    // 0 write 1 = 0
    s_state.storage[start_addr - s_state.offset + i] &= buffer[i];
  }
}

//! @param block_size must be a power of two
static void erase_block(uint32_t block_addr, uint32_t block_size) {
  ++s_state.erase_count;

  const uint32_t block_mask = ~(block_size - 1);
  uint32_t block_start = block_addr & block_mask;

  cl_assert(block_start >= s_state.offset);
  if (block_start + block_size > s_state.offset + s_state.length) {
    printf("-0x%x 0x%x\n", block_start + block_size,  s_state.offset + s_state.length);
  }
  cl_assert(block_start + block_size <= s_state.offset + s_state.length);

  memset(&s_state.storage[block_start - s_state.offset], 0xff, block_size);
}

void flash_erase_sector_blocking(uint32_t sector_addr) {
#if PLATFORM_SNOWY
  if (sector_addr <= BOTTOM_BOOT_REGION_END) {
    erase_block(sector_addr, BOTTOM_BOOT_SECTOR_SIZE);
    return;
  }
#endif
  erase_block(sector_addr, SECTOR_SIZE_BYTES);
}

uint32_t flash_get_subsector_base_address(uint32_t flash_addr) {
  return flash_addr & ~(SUBSECTOR_SIZE_BYTES - 1);
}

void flash_erase_subsector_blocking(uint32_t subsector_addr) {
  erase_block(subsector_addr, SUBSECTOR_SIZE_BYTES);
}

uint32_t flash_get_sector_base_address(uint32_t flash_addr) {
#if PLATFORM_SNOWY
  if (flash_addr <= BOTTOM_BOOT_REGION_END) {
    return (flash_addr & ~(BOTTOM_BOOT_SECTOR_SIZE - 1));
  }
#endif

  return (flash_addr & ~(SECTOR_SIZE_BYTES - 1));
}

uint32_t fake_flash_write_count(void) {
  return s_state.write_count;
}

uint32_t fake_flash_erase_count(void) {
  return s_state.erase_count;
}
