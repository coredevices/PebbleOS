/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "util/buffer.h"

#include "clar.h"

#include "stubs_logging.h"
#include "stubs_passert.h"
#include "stubs_pbl_malloc.h"

#include <string.h>

static const char* test_data = "This is a very complicated case, Maude.";

void test_buffer__should_add_data_until_full(void) {
  const size_t buffer_size = 101;

  Buffer* b = buffer_create(buffer_size);

  int bytes_written = 0;
  int num_elements = buffer_size / sizeof(test_data);
  for (int i = 0; i < num_elements; ++i) {
    cl_assert_equal_i(b->bytes_written, i * sizeof(test_data));
    cl_assert_equal_i(buffer_get_bytes_remaining(b), buffer_size - (i * sizeof(test_data)));
    bytes_written += buffer_add(b, (uint8_t *) test_data, sizeof(test_data));
    cl_assert_equal_i(bytes_written, (i + 1) * sizeof(test_data));
    cl_assert_equal_i(buffer_get_bytes_remaining(b), buffer_size - ((i+1) * sizeof(test_data)));
  }

  cl_assert(buffer_get_bytes_remaining(b) > 0);
  bytes_written = buffer_add(b, (uint8_t *) test_data, sizeof(test_data));
  cl_assert_equal_i(bytes_written, 0);

  free(b);
}

void test_buffer__cannot_remove_beyond_written(void) {
  Buffer *b = buffer_create(5);
  cl_assert_equal_i(0, buffer_remove(b, 0, 0));
  cl_assert_passert(buffer_remove(b, 0, 1));

  uint8_t b1 = 1;
  buffer_add(b, &b1, sizeof(uint8_t));
  cl_assert_passert(buffer_remove(b, 0, 2));
  cl_assert_equal_i(1, b->bytes_written);

  cl_assert_equal_i(1, buffer_remove(b, 0, 1));
  cl_assert_equal_i(0, b->bytes_written);

  free(b);
}

void test_buffer__can_remove(void) {
  uint8_t b1 = 1;
  uint8_t b2 = 2;
  uint8_t b3 = 3;
  uint8_t b4 = 4;

  Buffer *b = buffer_create(5);
  // works on empty buffer
  cl_assert_equal_i(0, buffer_remove(b, 0, 0));

  buffer_add(b, &b1, sizeof(uint8_t));
  buffer_add(b, &b2, sizeof(uint8_t));
  buffer_add(b, &b3, sizeof(uint8_t));
  buffer_add(b, &b4, sizeof(uint8_t));

  // handles out of bounds cases
  cl_assert_passert(buffer_remove(b, 0, 5));
  cl_assert_passert(buffer_remove(b, 1, 4));

  cl_assert_equal_i(4, b->bytes_written);
  cl_assert_equal_i(b->data[0], b1);
  cl_assert_equal_i(b->data[1], b2);
  cl_assert_equal_i(b->data[2], b3);
  cl_assert_equal_i(b->data[3], b4);

  // moves removed remaining bytes to close the gap
  cl_assert_equal_i(2, buffer_remove(b, 1*sizeof(uint8_t), 2*sizeof(uint8_t)));
  cl_assert_equal_i(2, b->bytes_written);
  cl_assert_equal_i(b->data[0], b1);
  cl_assert_equal_i(b->data[1], b4);

  free(b);
}

void test_buffer__can_remove_interior_data(void) {
  uint8_t b1 = 1;
  uint8_t b2 = 2;
  uint8_t b3 = 3;
  uint8_t b4 = 4;

  Buffer *b = buffer_create(4);
  buffer_add(b, &b1, sizeof(uint8_t));
  buffer_add(b, &b2, sizeof(uint8_t));
  buffer_add(b, &b3, sizeof(uint8_t));
  buffer_add(b, &b4, sizeof(uint8_t));

  // removing second element shifts elements three and four to overwrite it
  cl_assert_equal_i(sizeof(uint8_t), buffer_remove(b, 1 * sizeof(uint8_t), sizeof(uint8_t)));
  cl_assert_equal_i(b->bytes_written, 3);
  cl_assert_equal_i(b->data[0], b1);
  cl_assert_equal_i(b->data[1], b3);
  cl_assert_equal_i(b->data[2], b4);

  free(b);
}

void test_buffer__can_read_and_write_uint32(void) {
  uint32_t expected = 0x12345678;

  Buffer *b = buffer_create(4);

  buffer_add(b, (const uint8_t* const)&expected, sizeof(expected));
  cl_assert_equal_i(expected, *(uint32_t*)b->data);

  free(b);
}
