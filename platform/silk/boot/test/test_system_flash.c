/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "clar.h"

#include "drivers/system_flash.h"

#define KiB *1024

// Set bits n..0 in a bit-vector (zero-indexed)
#define BITS(n) ((((uint32_t)(1 << ((n) + 1)))) - 1)
// Set bits y..x in a bit-vector (x <= y; x, y >= 0)
#define BITS_BETWEEN(x, y) (BITS(y) & ~BITS(x-1))

// Yo dawg, I heard you like tests so I put tests in your tests so you can test
// your tests while you test!
void test_system_flash__bit_range_macros(void) {
  cl_assert_equal_i(0b1, BITS(0));
  cl_assert_equal_i(0b00011111, BITS(4));
  cl_assert_equal_i(0b00111111, BITS_BETWEEN(0, 5));
  cl_assert_equal_i(0b00111000, BITS_BETWEEN(3, 5));
  cl_assert_equal_i(0b00010000, BITS_BETWEEN(4, 4));
}

// Flash memory is organized into twelve sectors of unequal sizes.
// Sectors 0-3 are 16 KiB. Sector 4 is 64 KiB.
// The remaining sectors are 128 KiB.

// Bitset of sectors that have been "erased"
static uint32_t erased_sector;
static bool flash_locked, flash_flags_set;
static FLASH_Status return_status;
uint8_t *flash_written_data;
bool *flash_written_flag;
void *source_buffer;
uint32_t flash_data_start, flash_data_length;
bool callback_called;

void test_system_flash__initialize(void) {
  erased_sector = 0;
  flash_locked = true;
  flash_flags_set = false;
  return_status = FLASH_COMPLETE;
  flash_written_data = NULL;
  flash_written_flag = NULL;
  flash_data_start = 0;
  flash_data_length = 0;
  callback_called = false;
}

void test_system_flash__cleanup(void) {
  free(flash_written_data);
  free(flash_written_flag);
}

void test_system_flash__erase_zero_bytes(void) {
  cl_assert(system_flash_erase(FLASH_BASE, 0, NULL, NULL));
  cl_assert_equal_i(0, erased_sector);
  cl_assert(flash_locked);
}

void test_system_flash__erase_one_byte(void) {
  cl_assert(system_flash_erase(FLASH_BASE, 1, NULL, NULL));
  cl_assert_equal_i(BITS_BETWEEN(0, 0), erased_sector);
  cl_assert(flash_locked);
}

void test_system_flash__erase_one_byte_in_middle_of_sector(void) {
  cl_assert(system_flash_erase(FLASH_BASE + 12345, 1, NULL, NULL));
  cl_assert_equal_i(BITS_BETWEEN(0, 0), erased_sector);
  cl_assert(flash_locked);
}

void test_system_flash__erase_some_sectors_from_beginning(void) {
  cl_assert(system_flash_erase(FLASH_BASE, 128 KiB, NULL, NULL));
  cl_assert_equal_i(BITS_BETWEEN(0, 4), erased_sector);
  cl_assert(flash_locked);
}

void test_system_flash__erase_full_flash(void) {
  cl_assert(system_flash_erase(FLASH_BASE, 1024 KiB, NULL, NULL));
  cl_assert_equal_i(BITS_BETWEEN(0, 7), erased_sector);
  cl_assert(flash_locked);
}

void test_system_flash__erase_sector_0(void) {
  cl_assert(system_flash_erase(FLASH_BASE, 16 KiB, NULL, NULL));
  cl_assert_equal_i(BITS_BETWEEN(0, 0), erased_sector);
  cl_assert(flash_locked);
}

void test_system_flash__erase_16KB_sectors(void) {
  cl_assert(system_flash_erase(FLASH_BASE, 48 KiB, NULL, NULL));
  cl_assert_equal_i(BITS_BETWEEN(0, 2), erased_sector);
  cl_assert(flash_locked);
}

void callback_is_called_cb(uint32_t num, uint32_t den, void *context) {
  callback_called = true;
  cl_assert_equal_i(8675309, (uintptr_t)context);
}

void test_system_flash__callback_is_called(void) {
  cl_assert(system_flash_erase(FLASH_BASE, 16 KiB, callback_is_called_cb,
            (void *)8675309));
  cl_assert(callback_called);
  cl_assert(flash_locked);
}

void test_system_flash__handle_erase_error(void) {
  return_status = FLASH_ERROR_OPERATION;
  cl_assert(!system_flash_erase(FLASH_BASE, 16 KiB, NULL, NULL));
  cl_assert(flash_locked);
}

void error_in_middle_cb(uint32_t num, uint32_t den, void *context) {
  int *countdown = context;
  if ((*countdown)-- == 0) {
    return_status = FLASH_ERROR_OPERATION;
  }
}

void test_system_flash__handle_erase_error_mid_operation(void) {
  int countdown = 3;
  cl_assert(!system_flash_erase(FLASH_BASE, 512 KiB, error_in_middle_cb,
            &countdown));
  cl_assert(flash_locked);
  cl_assert_(countdown <= 0, "Callback not called enough times");
}


void malloc_flash_data(uint32_t size) {
  flash_data_length = size;
  flash_written_data = malloc(size * sizeof(uint8_t));
  cl_assert(flash_written_data);
  flash_written_flag = malloc(size * sizeof(bool));
  cl_assert(flash_written_flag);
}

void assert_flash_unwritten(uint32_t start, uint32_t length) {
  for (uint32_t i = 0; i < length; ++i) {
    cl_assert(flash_written_flag[start + i] == false);
  }
}

void test_system_flash__write_simple(void) {
  const char testdata[] = "The quick brown fox jumps over the lazy dog.";
  malloc_flash_data(100);
  flash_data_start = FLASH_BASE;
  cl_assert(system_flash_write(
        FLASH_BASE + 10, testdata, sizeof(testdata), callback_is_called_cb,
        (void *)8675309));
  cl_assert(flash_locked);
  cl_assert_equal_s(testdata, (const char *)&flash_written_data[10]);
  assert_flash_unwritten(0, 10);
  assert_flash_unwritten(10 + sizeof(testdata), 90 - sizeof(testdata));
  cl_assert(callback_called);
}

void test_system_flash__write_error(void) {
  return_status = FLASH_ERROR_OPERATION;
  malloc_flash_data(10);
  flash_data_start = FLASH_BASE;
  cl_assert(!system_flash_write(FLASH_BASE, "abc", 3, NULL, NULL));
  cl_assert(flash_locked);
  assert_flash_unwritten(0, 10);
}

extern void FLASH_Lock(void) {
  flash_locked = true;
}

extern void FLASH_Unlock(void) {
  flash_locked = false;
}

extern void FLASH_ClearFlag(uint32_t FLASH_FLAG) {
  flash_flags_set = false;
}

extern FLASH_Status FLASH_EraseSector(uint32_t sector, uint8_t voltage_range) {
  // Pretty sure FLASH_Sector_N defines are simply 8*N, at least for the first
  // twelve sectors.
  cl_assert_(!flash_locked, "Attempted to erase a locked flash");
  cl_assert_(IS_FLASH_SECTOR(sector), "Sector number out of range");
  cl_assert(IS_VOLTAGERANGE(voltage_range));
  cl_check_(flash_flags_set == false, "Forgot to clear flags before erasing");
  cl_check_((erased_sector & (1 << sector/8)) == 0,
            "Re-erasing an already erased sector");
  flash_flags_set = true;
  if (return_status == FLASH_COMPLETE) {
    erased_sector |= (1 << sector/8);
  }
  return return_status;
}

extern FLASH_Status FLASH_ProgramByte(uint32_t address, uint8_t data) {
  cl_assert_(!flash_locked, "Attempted to write to a locked flash");
  cl_assert_(address >= flash_data_start &&
             address < flash_data_start + flash_data_length,
             "Address out of range");
  cl_assert_(flash_written_flag[address - flash_data_start] == false,
             "Overwriting an already-written byte");
  if (return_status == FLASH_COMPLETE) {
    flash_written_data[address - flash_data_start] = data;
  }
  return return_status;
}

extern void dbgserial_print(char *str) {
  fprintf(stderr, "%s", str);
}

extern void dbgserial_print_hex(uint32_t num) {
  fprintf(stderr, "0x%.08x", num);
}

extern void dbgserial_putstr(char *str) {
  fprintf(stderr, "%s\n", str);
}
