/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "fake_gatt_client_operations.h"

#include <string.h>

#include <pbl/util/list.h>

#include <clar_asserts.h>

typedef struct {
  ListNode node;
  pbl_bt_characteristic_t characteristic;
} Read;

static Read *s_read_head;

static int s_read_return_value;

int gatt_client_op_read(pbl_bt_characteristic_t characteristic) {
  if (s_read_return_value != 0) {
    return s_read_return_value;
  }
  Read *read = malloc(sizeof(Read));
  *read = (const Read){
    .characteristic = characteristic,
  };
  if (s_read_head) {
    list_append((ListNode *)s_read_head, &read->node);
  } else {
    s_read_head = read;
  }
  return 0;
}

typedef struct {
  ListNode node;
  pbl_bt_characteristic_t characteristic;
  uint8_t *value;
  size_t value_length;
  bool is_response_required;
} Write;

static Write *s_write_head;

static int s_write_return_value;

static int fake_gatt_client_write(pbl_bt_characteristic_t characteristic, const uint8_t *value,
                                  size_t value_length, bool is_response_required) {
  if (s_write_return_value != 0) {
    return s_write_return_value;
  }
  Write *write = malloc(sizeof(Write));
  uint8_t *buffer;
  if (value_length) {
    cl_assert(value);
    buffer = malloc(value_length);
    memcpy(buffer, value, value_length);
  } else {
    cl_assert_equal_p(value, nullptr);
    buffer = nullptr;
  }
  *write = (const Write){
    .characteristic = characteristic,
    .is_response_required = is_response_required,
    .value = buffer,
    .value_length = value_length,
  };
  if (s_write_head) {
    list_append((ListNode *)s_write_head, &write->node);
  } else {
    s_write_head = write;
  }
  return 0;
}

int gatt_client_op_write(pbl_bt_characteristic_t characteristic, const uint8_t *value,
                         size_t value_length) {
  return fake_gatt_client_write(characteristic, value, value_length,
                                true /* is_response_required */);
}

int gatt_client_op_write_without_response(pbl_bt_characteristic_t characteristic,
                                          const uint8_t *value, size_t value_length) {
  return fake_gatt_client_write(characteristic, value, value_length,
                                false /* is_response_required */);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Fake Manipulation

void fake_gatt_client_op_init(void) {
  s_read_return_value = 0;
  s_write_return_value = 0;
}

void fake_gatt_client_op_deinit(void) {
  Read *read = s_read_head;
  while (read) {
    Read *next = (Read *)read->node.next;
    free(read);
    read = next;
  }
  s_read_head = nullptr;

  fake_gatt_client_op_clear_write_list();
}

void fake_gatt_client_op_set_read_return_value(int ret) {
  s_read_return_value = ret;
}

void fake_gatt_client_op_assert_read(pbl_bt_characteristic_t characteristic) {
  if (s_read_head) {
    cl_assert_equal_i(characteristic, s_read_head->characteristic);
  } else {
    cl_assert_(false, "No gatt_client_op_read() has happened at all");
  }
  Read *old_head = s_read_head;
  s_read_head = (Read *)list_pop_head(&s_read_head->node);
  free(old_head);
}

void fake_gatt_client_op_set_write_return_value(int ret) {
  s_write_return_value = ret;
}

void fake_gatt_client_op_clear_write_list(void) {
  Write *write = s_write_head;
  while (write) {
    Write *next = (Write *)write->node.next;
    free(write->value);
    free(write);
    write = next;
  }
  s_write_head = nullptr;
}

void fake_gatt_client_op_assert_no_write(void) {
  cl_assert_equal_p(s_write_head, nullptr);
}

static void fake_gatt_client_op_assert_write_failed(void) {
  cl_assert_(false,
             "No gatt_client_op_write() or "
             "gatt_client_op_write_without_response() has happened at all");
}

void fake_gatt_client_op_assert_write(pbl_bt_characteristic_t characteristic, const uint8_t *value,
                                      size_t value_length, bool is_response_required) {
  if (s_write_head) {
    cl_assert_equal_i(characteristic, s_write_head->characteristic);
    cl_assert_equal_i(s_write_head->value_length, value_length);
    cl_assert_equal_i(memcmp(value, s_write_head->value, value_length), 0);
    cl_assert_equal_b(is_response_required, s_write_head->is_response_required);
  } else {
    fake_gatt_client_op_assert_write_failed();
  }
  Write *old_write = s_write_head;
  s_write_head = (Write *)list_pop_head(&s_write_head->node);
  free(old_write->value);
  free(old_write);
}
