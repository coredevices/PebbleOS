/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "util/shared_circular_buffer.h"

#include "clar.h"

#include <string.h>

#include "stubs_passert.h"


// Stubs
///////////////////////////////////////////////////////////
int g_pbl_log_level = 0;
void pbl_log(int level, const char* src_filename, int src_line_number, const char* fmt, ...) { }


void test_shared_circular_buffer__initialize(void) {
}

void test_shared_circular_buffer__cleanup(void) {
}

static void prv_read_and_consume(SharedCircularBuffer *buffer, SharedCircularBufferClient *client,
                          uint8_t *data, uint32_t num_bytes) {
  while (num_bytes) {
    uint16_t chunk;
    const uint8_t *read_ptr;

    cl_assert(shared_circular_buffer_read(buffer, client, num_bytes, &read_ptr, &chunk));
    memcpy(data, read_ptr, chunk);
    cl_assert(shared_circular_buffer_consume(buffer, client, chunk));

    buffer += chunk;
    num_bytes -= chunk;
  }
}

void test_shared_circular_buffer__one_client(void) {
  SharedCircularBuffer buffer;
  uint8_t storage[9];
  shared_circular_buffer_init(&buffer, storage, sizeof(storage));

  const uint8_t* out_buffer;
  uint16_t out_length;

  // Add a client
  SharedCircularBufferClient client = (SharedCircularBufferClient) {};
  shared_circular_buffer_add_client(&buffer, &client);

  // We should start out empty
  cl_assert(!shared_circular_buffer_read(&buffer, &client, 1, &out_buffer, &out_length));

  cl_assert(shared_circular_buffer_write(&buffer, (uint8_t*) "123", 3, false));
  cl_assert_equal_i(shared_circular_buffer_get_write_space_remaining(&buffer), 5);
  cl_assert(shared_circular_buffer_write(&buffer, (uint8_t*) "456", 3, false));
  cl_assert_equal_i(shared_circular_buffer_get_write_space_remaining(&buffer), 2);
  cl_assert(!shared_circular_buffer_write(&buffer, (uint8_t*) "789", 3, false)); // too big
  cl_assert_equal_i(shared_circular_buffer_get_write_space_remaining(&buffer), 2);

  cl_assert(shared_circular_buffer_read(&buffer, &client, 4, &out_buffer, &out_length));
  cl_assert_equal_i(out_length, 4);
  cl_assert(memcmp(out_buffer, "1234", 4) == 0);
  cl_assert_equal_i(shared_circular_buffer_get_write_space_remaining(&buffer), 2);

  cl_assert(shared_circular_buffer_consume(&buffer, &client, 4));
  cl_assert_equal_i(shared_circular_buffer_get_write_space_remaining(&buffer), 6);

  // Now there's just 56 in the buffer. Fill it to the brim
  cl_assert(shared_circular_buffer_write(&buffer, (uint8_t*) "789", 3, false));
  cl_assert_equal_i(shared_circular_buffer_get_write_space_remaining(&buffer), 3);
  cl_assert(shared_circular_buffer_write(&buffer, (uint8_t*) "abc", 3, false));
  cl_assert_equal_i(shared_circular_buffer_get_write_space_remaining(&buffer), 0);
  cl_assert(!shared_circular_buffer_write(&buffer, (uint8_t*) "d", 1, false)); // too full
  cl_assert_equal_i(shared_circular_buffer_get_write_space_remaining(&buffer), 0);

  // Try a wrapped read
  cl_assert(shared_circular_buffer_read(&buffer, &client, 6, &out_buffer, &out_length));
  cl_assert_equal_i(out_length, 5);
  cl_assert(memcmp(out_buffer, (uint8_t*) "56789", 5) == 0);
  cl_assert(shared_circular_buffer_consume(&buffer, &client, 5));

  // Get the rest of the wrapped read
  cl_assert(shared_circular_buffer_read(&buffer, &client, 1, &out_buffer, &out_length));
  cl_assert_equal_i(out_length, 1);
  cl_assert(memcmp(out_buffer, (uint8_t*) "a", 1) == 0);
  cl_assert(shared_circular_buffer_consume(&buffer, &client, 1));

  // Consume one without reading it
  cl_assert(shared_circular_buffer_consume(&buffer, &client, 1));

  // Read the last little bit
  cl_assert(shared_circular_buffer_read(&buffer, &client, 1, &out_buffer, &out_length));
  cl_assert_equal_i(out_length, 1);
  cl_assert(memcmp(out_buffer, (uint8_t*) "c", 1) == 0);
  cl_assert(shared_circular_buffer_consume(&buffer, &client, 1));

  // And we should be empty
  cl_assert(!shared_circular_buffer_read(&buffer, &client, 1, &out_buffer, &out_length));
  cl_assert(!shared_circular_buffer_consume(&buffer, &client, 1));
}


void test_shared_circular_buffer__two_clients(void) {
  SharedCircularBuffer buffer;
  uint8_t storage[9];
  shared_circular_buffer_init(&buffer, storage, sizeof(storage));

  const uint8_t* out_buffer;
  uint16_t out_length;

  // Add clients
  SharedCircularBufferClient client1 = (SharedCircularBufferClient) {};
  shared_circular_buffer_add_client(&buffer, &client1);
  SharedCircularBufferClient client2 = (SharedCircularBufferClient) {};
  shared_circular_buffer_add_client(&buffer, &client2);

  // We should start out empty
  cl_assert(!shared_circular_buffer_read(&buffer, &client1, 1, &out_buffer, &out_length));
  cl_assert(!shared_circular_buffer_read(&buffer, &client2, 1, &out_buffer, &out_length));

  // Fill with data
  cl_assert(shared_circular_buffer_write(&buffer, (uint8_t*) "123456", 6, false));

  // Read different amounts from each client
  cl_assert(shared_circular_buffer_read(&buffer, &client1, 4, &out_buffer, &out_length));
  cl_assert(shared_circular_buffer_consume(&buffer, &client1, 4));
  cl_assert(memcmp(out_buffer, "1234", 4) == 0);

  cl_assert_equal_i(shared_circular_buffer_get_write_space_remaining(&buffer), 2);

  cl_assert(shared_circular_buffer_read(&buffer, &client2, 4, &out_buffer, &out_length));
  cl_assert(shared_circular_buffer_consume(&buffer, &client2, 4));
  cl_assert(memcmp(out_buffer, "1234", 4) == 0);
  cl_assert_equal_i(shared_circular_buffer_get_write_space_remaining(&buffer), 6);


  // Make client2 fall behind
  cl_assert(shared_circular_buffer_write(&buffer, (uint8_t*) "abcdef", 6, false));
  cl_assert_equal_i(shared_circular_buffer_get_write_space_remaining(&buffer), 0);

  cl_assert(shared_circular_buffer_read(&buffer, &client1, 3, &out_buffer, &out_length));
  cl_assert_equal_i(out_length, 3);
  cl_assert(memcmp(out_buffer, "56a", 3) == 0);
  cl_assert(shared_circular_buffer_consume(&buffer, &client1, 3));

  cl_assert(shared_circular_buffer_read(&buffer, &client1, 2, &out_buffer, &out_length));
  cl_assert_equal_i(out_length, 2);
  cl_assert(memcmp(out_buffer, "bc", 2) == 0);
  cl_assert(shared_circular_buffer_consume(&buffer, &client1, 2));

  cl_assert_equal_i(shared_circular_buffer_get_write_space_remaining(&buffer), 0);


  // Should fail, not enough room because client 2 is full
  cl_assert(!shared_circular_buffer_write(&buffer, (uint8_t*) "gh", 2, false));

  // This should pass and reset client 2's read index
  cl_assert(shared_circular_buffer_write(&buffer, (uint8_t*) "gh", 2, true));
  cl_assert_equal_i(shared_circular_buffer_get_write_space_remaining(&buffer), 3);
  cl_assert_equal_i(shared_circular_buffer_get_read_space_remaining(&buffer, &client1), 5);
  cl_assert_equal_i(shared_circular_buffer_get_read_space_remaining(&buffer, &client2), 2);


  // Make client2 fall behind again
  cl_assert(shared_circular_buffer_write(&buffer, (uint8_t*) "abc", 3, false));
  cl_assert_equal_i(shared_circular_buffer_get_write_space_remaining(&buffer), 0);

  cl_assert(shared_circular_buffer_read(&buffer, &client1, 3, &out_buffer, &out_length));
  cl_assert_equal_i(out_length, 3);
  cl_assert(memcmp(out_buffer, "def", 3) == 0);
  cl_assert(shared_circular_buffer_consume(&buffer, &client1, 3));

  cl_assert(shared_circular_buffer_read(&buffer, &client1, 2, &out_buffer, &out_length));
  cl_assert_equal_i(out_length, 2);
  cl_assert(memcmp(out_buffer, "gh", 2) == 0);
  cl_assert(shared_circular_buffer_consume(&buffer, &client1, 2));

  cl_assert_equal_i(shared_circular_buffer_get_read_space_remaining(&buffer, &client1), 3);
  cl_assert_equal_i(shared_circular_buffer_get_read_space_remaining(&buffer, &client2), 5);
  cl_assert_equal_i(shared_circular_buffer_get_write_space_remaining(&buffer), 3);

  // If we remove client2, it should create more space
  shared_circular_buffer_remove_client(&buffer, &client2);
  cl_assert_equal_i(shared_circular_buffer_get_write_space_remaining(&buffer), 5);
}


void test_shared_circular_buffer__corner_case(void) {
  SharedCircularBuffer buffer;
  uint8_t storage[4];
  shared_circular_buffer_init(&buffer, storage, sizeof(storage));

  // Add a client
  SharedCircularBufferClient client = (SharedCircularBufferClient) {};
  shared_circular_buffer_add_client(&buffer, &client);

  // We should start out empty
  cl_assert_equal_i(shared_circular_buffer_get_read_space_remaining(&buffer, &client), 0);

  // Write 2
  cl_assert(shared_circular_buffer_write(&buffer, storage, 2, false));
  cl_assert_equal_i(shared_circular_buffer_get_read_space_remaining(&buffer, &client), 2);

  // Consume it
  prv_read_and_consume(&buffer, &client, storage, 2);
  cl_assert_equal_i(shared_circular_buffer_get_read_space_remaining(&buffer, &client), 0);

  // Write 2 more
  cl_assert(shared_circular_buffer_write(&buffer, storage, 2, false));
  cl_assert_equal_i(shared_circular_buffer_get_read_space_remaining(&buffer, &client), 2);

  // Consume it
  prv_read_and_consume(&buffer, &client, storage, 2);
  cl_assert_equal_i(shared_circular_buffer_get_read_space_remaining(&buffer, &client), 0);
}


void test_shared_circular_buffer__subsampling_2of5(void) {
  SharedCircularBuffer buffer;
  uint16_t item_size = 2;
  uint8_t storage[12*item_size];
  uint8_t out_buffer[12*item_size];
  uint16_t items_read;

  shared_circular_buffer_init(&buffer, storage, sizeof(storage));
  SubsampledSharedCircularBufferClient client = {};
  shared_circular_buffer_add_subsampled_client(&buffer, &client, 2, 5);

  cl_assert(shared_circular_buffer_write(
      &buffer, (uint8_t*)"0a1b2c3d4e5f6g7h8i", 9*item_size, false));
  items_read = shared_circular_buffer_read_subsampled(
      &buffer, &client, item_size, out_buffer, 100);
  cl_assert_equal_i(items_read, 4);
  cl_assert_equal_m(out_buffer, "0a3d5f8i", 8);

  cl_assert(shared_circular_buffer_write(
      &buffer, (uint8_t*)"9j0k1m2n3o4p5q", 7*item_size, false));
  items_read = shared_circular_buffer_read_subsampled(
      &buffer, &client, item_size, out_buffer, 2);
  cl_assert_equal_i(items_read, 2);
  cl_assert_equal_m(out_buffer, "0k3o", 4);

  items_read = shared_circular_buffer_read_subsampled(
      &buffer, &client, item_size, out_buffer, 2);
  cl_assert_equal_i(items_read, 1);
  cl_assert_equal_m(out_buffer, "5q", 2);
}


void test_shared_circular_buffer__subsampling_1of3(void) {
  SharedCircularBuffer buffer;
  uint16_t item_size = 2;
  uint8_t storage[12*item_size];
  uint8_t out_buffer[12*item_size];
  uint16_t items_read;

  // Init
  shared_circular_buffer_init(&buffer, storage, sizeof(storage));
  SubsampledSharedCircularBufferClient client = {};
  shared_circular_buffer_add_subsampled_client(&buffer, &client, 1, 3);

  cl_assert(shared_circular_buffer_write(
      &buffer, (uint8_t*)"0a1b2c3d4e5f6g7h8i9j", 10*item_size, false));
  items_read = shared_circular_buffer_read_subsampled(
      &buffer, &client, item_size, out_buffer, 100);
  cl_assert_equal_i(items_read, 4);
  cl_assert_equal_m(out_buffer, "0a3d6g9j", 8);

  cl_assert(shared_circular_buffer_write(
      &buffer, (uint8_t*)"0k1m2n", 3*item_size, false));
  items_read = shared_circular_buffer_read_subsampled(
      &buffer, &client, item_size, out_buffer, 100);
  cl_assert_equal_i(items_read, 1);
  cl_assert_equal_m(out_buffer, "2n", 2);
}


void test_shared_circular_buffer__subsampling_1of1(void) {
  SharedCircularBuffer buffer;
  uint16_t item_size = 2;
  uint8_t storage[12*item_size];
  uint8_t out_buffer[12*item_size];
  uint16_t items_read;

  // Init
  shared_circular_buffer_init(&buffer, storage, sizeof(storage));
  SubsampledSharedCircularBufferClient client = {};
  shared_circular_buffer_add_subsampled_client(&buffer, &client, 3, 3);

  // No subsampling
  cl_assert(shared_circular_buffer_write(
      &buffer, (uint8_t*)"0a1b2c3d4e5f6g7h8i9j", 10*item_size, false));
  items_read = shared_circular_buffer_read_subsampled(
      &buffer, &client, item_size, out_buffer, 9);
  cl_assert_equal_i(items_read, 9);
  cl_assert_equal_m(out_buffer, "0a1b2c3d4e5f6g7h8i", 18);

  cl_assert(shared_circular_buffer_write(
      &buffer, (uint8_t*)"0k1m", 2*item_size, false));
  items_read = shared_circular_buffer_read_subsampled(
      &buffer, &client, item_size, out_buffer, 100);
  cl_assert_equal_i(items_read, 3);
  cl_assert_equal_m(out_buffer, "9j0k1m", 6);
}


void test_shared_circular_buffer__subsampling_variable_ratio(void) {
  SharedCircularBuffer buffer;
  uint16_t item_size = 2;
  uint8_t storage[12*item_size];
  uint8_t out_buffer[12*item_size];

  shared_circular_buffer_init(&buffer, storage, sizeof(storage));
  SubsampledSharedCircularBufferClient client = {};
  shared_circular_buffer_add_subsampled_client(&buffer, &client, 1, 2);

  cl_assert(shared_circular_buffer_write(
      &buffer, (uint8_t*)"0a1b2c3d4e5f6g7h8i9j", 10*item_size, false));
  // Consume "0a1b2c3d4e"
  cl_assert_equal_i(shared_circular_buffer_read_subsampled(
      &buffer, &client, item_size, out_buffer, 3), 3);
  cl_assert_equal_m(out_buffer, "0a2c4e", 6);

  subsampled_shared_circular_buffer_client_set_ratio(&client, 2, 3);
  // Consume "5f6g7h8i"
  cl_assert_equal_i(shared_circular_buffer_read_subsampled(
      &buffer, &client, item_size, out_buffer, 3), 3);
  // Normally the next read would skip 5f, but changing the ratio resets the
  // subsampling state and the first sample after resetting the state is never
  // skipped.
  cl_assert_equal_m(out_buffer, "5f7h8i", 6);
}


void test_shared_circular_buffer__subsampling_set_ratio_is_idempotent(void) {
  SharedCircularBuffer buffer;
  uint16_t item_size = 2;
  uint8_t storage[12*item_size];
  uint8_t out_buffer[12*item_size];

  shared_circular_buffer_init(&buffer, storage, sizeof(storage));
  SubsampledSharedCircularBufferClient client = {};
  shared_circular_buffer_add_subsampled_client(&buffer, &client, 1, 2);

  cl_assert(shared_circular_buffer_write(
      &buffer, (uint8_t*)"0a1b2c3d4e5f6g7h8i9j", 10*item_size, false));
  // Consume "0a1b2c3d4e"
  cl_assert_equal_i(shared_circular_buffer_read_subsampled(
      &buffer, &client, item_size, out_buffer, 3), 3);
  cl_assert_equal_m(out_buffer, "0a2c4e", 6);

  // This should be a no-op. the "5f" sample should still be skipped on the next
  // read.
  subsampled_shared_circular_buffer_client_set_ratio(&client, 1, 2);
  cl_assert_equal_i(shared_circular_buffer_read_subsampled(
      &buffer, &client, item_size, out_buffer, 1), 1);
  cl_assert_equal_m(out_buffer, "6g", 2);
}
