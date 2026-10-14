/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <clar.h>
#include <kernel/memory_layout.h>

// Tests
///////////////////////////////////////////////////////////
void test_memory_layout__pointer_in_region(void) {
  MpuRegion region = {.base_address = 1000, .size = 1000};

  cl_assert(!memory_layout_is_pointer_in_region(&region, (void *)0));
  cl_assert(!memory_layout_is_pointer_in_region(&region, (void *)999));
  cl_assert(memory_layout_is_pointer_in_region(&region, (void *)1000));
  cl_assert(memory_layout_is_pointer_in_region(&region, (void *)1500));
  cl_assert(memory_layout_is_pointer_in_region(&region, (void *)1999));
  cl_assert(!memory_layout_is_pointer_in_region(&region, (void *)2000));
  cl_assert(!memory_layout_is_pointer_in_region(&region, (void *)9999));
}

void test_memory_layout__cstring_in_region(void) {
  const char buffer[] = "yyyxxxstrstr\0badstrxxxyyy";
  const char *valid_str = buffer + 6; // skip yyyxxx, equal to "strstr"
  const char *invalid_str =
      buffer + 6 + 7; // skip yyyxxx + strstr\0, equal to "badstr" but no trailing null

  MpuRegion region = {
    .base_address = (uintptr_t)buffer + 3, // Skip leading yyy
    .size = sizeof(buffer) - 1 - 3 - 3     // Skip trailing null, leading yyy, trailing yyy
  };

  // In the leading y's region
  cl_assert(!memory_layout_is_cstring_in_region(&region, buffer, 10));

  // Valid string
  cl_assert(memory_layout_is_cstring_in_region(&region, valid_str, 10));
  // Max length is not long enough to find the null
  cl_assert(!memory_layout_is_cstring_in_region(&region, valid_str, 3));
  // Max length equal to the string length
  cl_assert(memory_layout_is_cstring_in_region(&region, valid_str, 6));
  // Max length that would wrap the address space
  cl_assert(memory_layout_is_cstring_in_region(&region, valid_str, SIZE_MAX));

  // Invalid string, there is a null after the y's but the y's should be after the end of the region
  cl_assert(!memory_layout_is_cstring_in_region(&region, invalid_str, 100));
  // Invalid string, no null.
  cl_assert(!memory_layout_is_cstring_in_region(&region, invalid_str, 3));
  cl_assert(!memory_layout_is_cstring_in_region(&region, invalid_str, SIZE_MAX));
}

void test_memory_layout__buffer_in_region(void) {
  MpuRegion region = {.base_address = 1000, .size = 1000};

  cl_assert(memory_layout_is_buffer_in_region(&region, (void *)1000, 1000));
  cl_assert(!memory_layout_is_buffer_in_region(&region, (void *)1000, 1001));
  cl_assert(!memory_layout_is_buffer_in_region(&region, (void *)999, 10));
  // Lengths that wrap back into the region
  cl_assert(!memory_layout_is_buffer_in_region(&region, (void *)1500, SIZE_MAX));
  cl_assert(!memory_layout_is_buffer_in_region(&region, (void *)1500, SIZE_MAX - 100));
}
