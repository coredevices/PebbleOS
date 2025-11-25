/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "util/dict.h"
#include "util/math.h"
#include "util/net.h"
#include "util/size.h"

#include "clar.h"

#include <string.h>
#include <stdbool.h>
#include <strings.h>

// Stubs
///////////////////////////////////////////////////////////
#include "stubs_logging.h"
#include "stubs_passert.h"
#include "stubs_pbl_malloc.h"

// Tests
///////////////////////////////////////////////////////////

void test_dict__initialize(void) {
}

void test_dict__cleanup(void) {
}

static const uint32_t SOME_DATA_KEY = 0xb00bf00b;
static const uint8_t SOME_DATA[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

static const uint32_t SOME_STRING_KEY = 0xbeefbabe;
static const char *SOME_STRING = "Hello World";

static const uint32_t SOME_NULL_KEY = 0x0;

static const uint32_t SOME_EMPTY_STRING_KEY = 0x1;
static const char *SOME_EMPTY_STRING = "";

static const uint32_t SOME_UINT8_KEY = 0x88888888;
static const uint32_t SOME_UINT16_KEY = 0x16161616;
static const uint32_t SOME_UINT32_KEY = 0x32323232;
static const uint32_t SOME_INT8_KEY = 0x11888888;
static const uint32_t SOME_INT16_KEY = 0x11161616;
static const uint32_t SOME_INT32_KEY = 0x11323232;

void test_dict__calc_size(void) {
  uint32_t size;
  size = dict_calc_buffer_size(0);
  cl_assert(size == sizeof(Dictionary));
  size = dict_calc_buffer_size(1, 1);
  cl_assert(size == sizeof(Dictionary) + sizeof(Tuple) + 1);
  size = dict_calc_buffer_size(3, 10, 100, 1000);
  cl_assert(size == sizeof(Dictionary) + (3 * sizeof(Tuple)) + 10 + 100 + 1000);
}

struct SerializeTestResult {
  bool okay;
  uint16_t expected_size;
};

static void serialize_callback(const uint8_t * const data, const uint16_t size, void *context) {
  struct SerializeTestResult *result = context;
  result->okay = true;
  result->expected_size = size;

  // Read back:
  DictionaryIterator iter;
  Tuple *tuple = dict_read_begin_from_buffer(&iter, data, size);
  cl_assert(tuple != NULL);
  cl_assert(iter.dictionary->count == 3);
}

void test_dict__tuplets_utils(void) {
  Tuplet tuplets[] = {
    TupletBytes(SOME_DATA_KEY, SOME_DATA, sizeof(SOME_DATA)),
    TupletCString(SOME_STRING_KEY, SOME_STRING),
    TupletInteger(SOME_UINT32_KEY, (uint32_t) 32),
  };
  const uint32_t size = dict_calc_buffer_size_from_tuplets(tuplets, 3);
  cl_assert(size == sizeof(Dictionary) + (3 * sizeof(Tuple)) + sizeof(SOME_DATA) + strlen(SOME_STRING) + 1 + sizeof(uint32_t));

  struct SerializeTestResult context = { .okay = false, .expected_size = size };
  DictionaryResult result = dict_serialize_tuplets(serialize_callback, &context,
      tuplets, 3);
  cl_assert(result == DICT_OK);
  cl_assert(context.okay == true);
  cl_assert(context.expected_size == size);
}

void test_dict__write_read(void) {
  // Stack allocated buffer:
  const uint8_t key_count = 10;
  const uint32_t size = dict_calc_buffer_size(key_count,
                                              sizeof(SOME_DATA),
                                              strlen(SOME_STRING) + 1,
                                              sizeof(uint8_t),
                                              sizeof(uint16_t),
                                              sizeof(uint32_t),
                                              sizeof(int8_t),
                                              sizeof(int16_t),
                                              sizeof(int32_t),
                                              0,
                                              strlen(SOME_EMPTY_STRING) + 1);
  const uint32_t surplus = 16; // allocate more than needed, see comment with the `final_size` test
  uint8_t buffer[size + surplus];

  // Write:
  DictionaryIterator iter;
  DictionaryResult result;
  result = dict_write_begin(&iter, buffer, sizeof(buffer));
  cl_assert(result == DICT_OK);

  result = dict_write_data(&iter, SOME_DATA_KEY, SOME_DATA, sizeof(SOME_DATA));
  cl_assert(result == DICT_OK);
  result = dict_write_cstring(&iter, SOME_STRING_KEY, SOME_STRING);
  cl_assert(result == DICT_OK);
  result = dict_write_uint8(&iter, SOME_UINT8_KEY, 8);
  cl_assert(result == DICT_OK);
  result = dict_write_uint16(&iter, SOME_UINT16_KEY, 16);
  cl_assert(result == DICT_OK);
  result = dict_write_uint32(&iter, SOME_UINT32_KEY, 32);
  cl_assert(result == DICT_OK);
  result = dict_write_int8(&iter, SOME_INT8_KEY, -8);
  cl_assert(result == DICT_OK);
  result = dict_write_int16(&iter, SOME_INT16_KEY, -16);
  cl_assert(result == DICT_OK);
  result = dict_write_int32(&iter, SOME_INT32_KEY, -32);
  cl_assert(result == DICT_OK);
  result = dict_write_cstring(&iter, SOME_NULL_KEY, NULL);
  cl_assert(result == DICT_OK);
  result = dict_write_cstring(&iter, SOME_EMPTY_STRING_KEY, SOME_EMPTY_STRING);
  cl_assert(result == DICT_OK);

  const uint32_t final_size = dict_write_end(&iter);
  cl_assert(result == DICT_OK);
  cl_assert(final_size == size);
  cl_assert(iter.dictionary->count == key_count);

  // Read:
  Tuple *tuple = dict_read_begin_from_buffer(&iter, buffer, final_size);
  uint8_t count = 0;
  bool data_found = false;
  bool string_found = false;
  bool uint8_found = false;
  bool uint16_found = false;
  bool uint32_found = false;
  bool int8_found = false;
  bool int16_found = false;
  bool int32_found = false;
  bool null_cstring_found = false;
  bool empty_cstring_found = false;
  while (tuple != NULL) {
    ++count;
    switch (tuple->key) {
      case SOME_DATA_KEY:
        cl_assert(tuple->length == sizeof(SOME_DATA));
        cl_assert(memcmp(tuple->value->data, SOME_DATA, sizeof(SOME_DATA)) == 0);
        data_found = true;
        break;
      case SOME_STRING_KEY:
        cl_assert(tuple->length == strlen(SOME_STRING) + 1);
        cl_assert(strncmp(tuple->value->cstring, SOME_STRING, strlen(SOME_STRING) + 1) == 0);
        // Check zero termination:
        cl_assert(tuple->value->cstring[strlen(SOME_STRING)] == 0);
        string_found = true;
        break;
      case SOME_UINT8_KEY:
        cl_assert(tuple->length == sizeof(uint8_t));
        cl_assert(tuple->value->uint8 == 8);
        uint8_found = true;
        break;
      case SOME_UINT16_KEY:
        cl_assert(tuple->length == sizeof(uint16_t));
        cl_assert(tuple->value->uint16 == 16);
        uint16_found = true;
        break;
      case SOME_UINT32_KEY:
        cl_assert(tuple->length == sizeof(uint32_t));
        cl_assert(tuple->value->uint32 == 32);
        uint32_found = true;
        break;
      case SOME_INT8_KEY:
        cl_assert(tuple->length == sizeof(int8_t));
        cl_assert(tuple->value->int8 == -8);
        int8_found = true;
        break;
      case SOME_INT16_KEY:
        cl_assert(tuple->length == sizeof(int16_t));
        cl_assert(tuple->value->int16 == -16);
        int16_found = true;
        break;
      case SOME_INT32_KEY:
        cl_assert(tuple->length == sizeof(int32_t));
        cl_assert(tuple->value->int32 == -32);
        int32_found = true;
        break;
      case SOME_NULL_KEY:
        cl_assert(tuple->length == 0);
        null_cstring_found = true;
        break;
      case SOME_EMPTY_STRING_KEY:
        cl_assert(tuple->length == strlen(SOME_EMPTY_STRING) + 1);
        cl_assert(strncmp(tuple->value->cstring, SOME_EMPTY_STRING, strlen(SOME_EMPTY_STRING) + 1) == 0);
        // Check zero termination:
        cl_assert(tuple->value->cstring[strlen(SOME_EMPTY_STRING)] == 0);
        empty_cstring_found = true;
        break;
    }
    tuple = dict_read_next(&iter);
  }
  cl_assert(count == key_count);
  cl_assert(data_found);
  cl_assert(string_found);
  cl_assert(uint8_found);
  cl_assert(uint16_found);
  cl_assert(uint32_found);
  cl_assert(int8_found);
  cl_assert(int16_found);
  cl_assert(int32_found);
  cl_assert(null_cstring_found);
  cl_assert(empty_cstring_found);
}

void test_dict__out_of_storage(void) {
  uint8_t buffer[1];
  DictionaryIterator iter;
  DictionaryResult result;
  result = dict_write_begin(&iter, buffer, 0);
  cl_assert(result == DICT_NOT_ENOUGH_STORAGE);
  result = dict_write_begin(&iter, buffer, sizeof(buffer));
  cl_assert(result == DICT_OK);
  result = dict_write_cstring(&iter, SOME_STRING_KEY, SOME_STRING);
  cl_assert(result == DICT_NOT_ENOUGH_STORAGE);
}

void test_dict__tuple_header_size(void) {
  Tuple t;
  t.type = 0;
  t.type = ~t.type;
  uint8_t num_bits = ffs(t.type + 1) - 1;
  cl_assert(num_bits % 8 == 0);
  // Test that the .value field isn't part of the header:
  cl_assert(sizeof(Tuple) == sizeof(t.key) + sizeof(t.length) + (num_bits / 8));
}

static void *CONTEXT = (void *)0xabcdabcd;
static const char *NEW_STRING = "Bye, bye, World";
static bool is_int8_updated = false;
static bool is_string_updated = false;
static bool should_update_existing_keys_only = false;
static bool test_not_enough_storage = false;
static bool is_data_updated = false;

static void update_key_callback(const uint32_t key, const Tuple *new_tuple, const Tuple *old_tuple, void *context) {
  cl_assert(CONTEXT == context);
  switch (key) {
    case SOME_INT8_KEY:
      is_int8_updated = true;
      cl_assert(should_update_existing_keys_only == false);
      cl_assert(new_tuple->type == TUPLE_INT);
      cl_assert(new_tuple->length == sizeof(int8_t));
      cl_assert(new_tuple->value->int8 == -3);
      cl_assert(old_tuple == NULL_TUPLE);
      break;
    case SOME_STRING_KEY:
      is_string_updated = true;
      cl_assert(new_tuple->type == TUPLE_CSTRING);
      cl_assert(new_tuple->length == strlen(NEW_STRING) + 1);
      cl_assert(strcmp(new_tuple->value->cstring, NEW_STRING) == 0);
      cl_assert(old_tuple->type == TUPLE_CSTRING);
      cl_assert(old_tuple->length == strlen(SOME_STRING) + 1);
      cl_assert(strcmp(old_tuple->value->cstring, SOME_STRING) == 0);
      break;
    case SOME_DATA_KEY:
      is_data_updated = true;
      cl_assert(new_tuple->type == TUPLE_BYTE_ARRAY);
      cl_assert(new_tuple->length == sizeof(SOME_DATA));
      cl_assert(old_tuple->type == TUPLE_BYTE_ARRAY);
      cl_assert(old_tuple->length == sizeof(SOME_DATA));
      cl_assert(memcmp(new_tuple->value->data, old_tuple->value->data, sizeof(SOME_DATA)) == 0);
      break;
    default:
      break;
  }
}

void test_dict__merge(void) {
  Tuplet dest_tuplets[] = {
    TupletBytes(SOME_DATA_KEY, SOME_DATA, sizeof(SOME_DATA)), // unchanged value
    TupletCString(SOME_STRING_KEY, SOME_STRING),
  };
  Tuplet source_tuplets[] = {
    TupletCString(SOME_STRING_KEY, NEW_STRING),
    TupletInteger(SOME_INT8_KEY, (int8_t) -3),
  };

  for (int i = 0; i < 3; ++i) {
    is_int8_updated = false;
    is_string_updated = false;
    is_data_updated = false;

    test_not_enough_storage = (i == 2);
    should_update_existing_keys_only = (i == 0) || test_not_enough_storage;

    uint32_t tmp_size = 0;
    const uint32_t source_size = dict_calc_buffer_size_from_tuplets(source_tuplets, ARRAY_LENGTH(source_tuplets));
    const uint32_t min_dest_size = dict_calc_buffer_size_from_tuplets(dest_tuplets, ARRAY_LENGTH(dest_tuplets));

    const uint32_t dest_size = test_not_enough_storage ? min_dest_size : min_dest_size + source_size;

    uint8_t source_buffer[source_size];
    tmp_size = source_size; // dict_serialize_tuplets_to_buffer modifies this.
    dict_serialize_tuplets_to_buffer(source_tuplets, ARRAY_LENGTH(source_tuplets), source_buffer, &tmp_size);
    DictionaryIterator source_iter;
    dict_read_begin_from_buffer(&source_iter, source_buffer, source_size);

    uint8_t dest_buffer[dest_size];
    tmp_size = dest_size; // dict_serialize_tuplets_to_buffer modifies this.
    dict_serialize_tuplets_to_buffer(dest_tuplets, ARRAY_LENGTH(dest_tuplets), dest_buffer, &tmp_size);
    DictionaryIterator dest_iter;
    dict_read_begin_from_buffer(&dest_iter, dest_buffer, dest_size);

    tmp_size = dest_size;
    dict_merge(&dest_iter, &tmp_size, &source_iter, should_update_existing_keys_only, update_key_callback, (void *) CONTEXT);
    cl_assert(is_int8_updated == !should_update_existing_keys_only);
    cl_assert(is_string_updated == !test_not_enough_storage);
    cl_assert(is_data_updated == !test_not_enough_storage);

    enum {
      INT8_IDX,
      STRING_IDX,
      DATA_IDX,
      NUM_TUPLES,
    };
    bool has_tuple[NUM_TUPLES] = { false, false, false };
    Tuple *tuple = dict_read_begin_from_buffer(&dest_iter, dest_buffer, tmp_size);
    while (tuple) {
      switch (tuple->key) {
        case SOME_DATA_KEY:
          has_tuple[DATA_IDX] = true;
          cl_assert(tuple->type == TUPLE_BYTE_ARRAY);
          cl_assert(tuple->length == sizeof(SOME_DATA));
          cl_assert(memcmp(tuple->value->data, SOME_DATA, sizeof(SOME_DATA)) == 0);
          break;
        case SOME_STRING_KEY:
          has_tuple[STRING_IDX] = true;
          cl_assert(tuple->type == TUPLE_CSTRING);
          if (test_not_enough_storage) {
            // If there is insufficient storage, we don't expect this tuple to
            // have been updated (since it can't fit!)
            cl_assert(tuple->length == strlen(SOME_STRING) + 1);
            cl_assert(strcmp(tuple->value->cstring, SOME_STRING) == 0);
          } else {
            cl_assert(tuple->length == strlen(NEW_STRING) + 1);
            cl_assert(strcmp(tuple->value->cstring, NEW_STRING) == 0);
          }
          break;
        case SOME_INT8_KEY:
          has_tuple[INT8_IDX] = true;
          cl_assert(should_update_existing_keys_only == false);
          cl_assert(tuple->type == TUPLE_INT);
          cl_assert(tuple->length == sizeof(int8_t));
          cl_assert(tuple->value->int8 == -3);
          break;
        default:
          break;
      }
      tuple = dict_read_next(&dest_iter);
    }
    if (test_not_enough_storage || should_update_existing_keys_only) {
      cl_assert(has_tuple[INT8_IDX] == false);
    } else {
      cl_assert(has_tuple[INT8_IDX] == true);
    }
    cl_assert(has_tuple[STRING_IDX] == true);
    cl_assert(has_tuple[DATA_IDX] == true);
  }
}
