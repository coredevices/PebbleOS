/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "fake_gatt_client_operations.h"

#include "comm/ble/gatt_client_operations.h"

#include "util/list.h"

#include "clar_asserts.h"

#include <string.h>

typedef struct {
  ListNode node;
  BLECharacteristic characteristic;
  GAPLEClient client;
} Read;

static Read *s_read_head;

static BTErrno s_read_return_value;

BTErrno gatt_client_op_read(BLECharacteristic characteristic,
                            GAPLEClient client) {
  if (s_read_return_value != BTErrnoOK) {
    return s_read_return_value;
  }
  Read *read = malloc(sizeof(Read));
  *read = (const Read) {
    .characteristic = characteristic,
    .client = client,
  };
  if (s_read_head) {
    list_append((ListNode *)s_read_head, &read->node);
  } else {
    s_read_head = read;
  }
  return BTErrnoOK;
}

void gatt_client_consume_read_response(uintptr_t object_ref,
                                       uint8_t value_out[],
                                       uint16_t value_length,
                                       GAPLEClient client) {
}

typedef struct {
  ListNode node;
  BLECharacteristic characteristic;
  GAPLEClient client;
  uint8_t *value;
  size_t value_length;
  bool is_response_required;
} Write;

static Write *s_write_head;

static BTErrno s_write_return_value;

static BTErrno fake_gatt_client_write(BLECharacteristic characteristic,
                                      const uint8_t *value,
                                      size_t value_length,
                                      GAPLEClient client,
                                      bool is_response_required) {
  if (s_write_return_value != BTErrnoOK) {
    return s_write_return_value;
  }
  Write *write = malloc(sizeof(Write));
  uint8_t *buffer;
  if (value_length) {
    cl_assert(value);
    buffer = malloc(value_length);
    memcpy(buffer, value, value_length);
  } else {
    cl_assert_equal_p(value, NULL);
    buffer = NULL;
  }
  *write = (const Write) {
    .characteristic = characteristic,
    .client = client,
    .is_response_required = is_response_required,
    .value = buffer,
    .value_length = value_length,
  };
  if (s_write_head) {
    list_append((ListNode *)s_write_head, &write->node);
  } else {
    s_write_head = write;
  }
  return BTErrnoOK;
}

BTErrno gatt_client_op_write(BLECharacteristic characteristic,
                             const uint8_t *value,
                             size_t value_length,
                             GAPLEClient client) {
  return fake_gatt_client_write(characteristic, value, value_length, client,
                                true /* is_response_required */);
}

BTErrno gatt_client_op_write_without_response(BLECharacteristic characteristic,
                                              const uint8_t *value,
                                              size_t value_length,
                                              GAPLEClient client) {
  return fake_gatt_client_write(characteristic, value, value_length, client,
                                false /* is_response_required */);
}

BTErrno gatt_client_op_write_descriptor(BLEDescriptor descriptor,
                                        const uint8_t *value,
                                        size_t value_length,
                                        GAPLEClient client) {
  return BTErrnoOK;
}

BTErrno gatt_client_op_read_descriptor(BLEDescriptor descriptor,
                                       GAPLEClient client) {
  return BTErrnoOK;
}

BTErrno gatt_client_op_write_descriptor_cccd(BLEDescriptor cccd, const uint16_t *value) {
  return BTErrnoOK;
}

void gatt_client_op_cleanup(GAPLEClient client) {

}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Fake Manipulation

void fake_gatt_client_op_init(void) {
  s_read_return_value = BTErrnoOK;
  s_write_return_value = BTErrnoOK;
}

void fake_gatt_client_op_deinit(void) {
  Read *read = s_read_head;
  while (read) {
    Read *next = (Read *) read->node.next;
    free(read);
    read = next;
  }
  s_read_head = NULL;

  fake_gatt_client_op_clear_write_list();
}

void fake_gatt_client_op_set_read_return_value(BTErrno e) {
  s_read_return_value = e;
}

void fake_gatt_client_op_assert_read(BLECharacteristic characteristic,
                                     GAPLEClient client) {
  if (s_read_head) {
    cl_assert_equal_i(characteristic, s_read_head->characteristic);
    cl_assert_equal_i(client, s_read_head->client);
  } else {
    cl_assert_(false, "No gatt_client_op_read() has happened at all");
  }
  Read *old_head = s_read_head;
  s_read_head = (Read *) list_pop_head(&s_read_head->node);
  free(old_head);
}

void fake_gatt_client_op_set_write_return_value(BTErrno e) {
  s_write_return_value = e;
}

void fake_gatt_client_op_clear_write_list(void) {
  Write *write = s_write_head;
  while (write) {
    Write *next = (Write *) write->node.next;
    free(write->value);
    free(write);
    write = next;
  }
  s_write_head = NULL;
}

void fake_gatt_client_op_assert_no_write(void) {
  cl_assert_equal_p(s_write_head, NULL);
}

static void fake_gatt_client_op_assert_write_failed(void) {
  cl_assert_(false, "No gatt_client_op_write() or "
             "gatt_client_op_write_without_response() has happened at all");
}

void fake_gatt_client_op_assert_write(BLECharacteristic characteristic,
                                      const uint8_t *value, size_t value_length,
                                      GAPLEClient client, bool is_response_required) {
  if (s_write_head) {
    cl_assert_equal_i(characteristic, s_write_head->characteristic);
    cl_assert_equal_i(s_write_head->value_length, value_length);
    cl_assert_equal_i(memcmp(value, s_write_head->value, value_length), 0);
    cl_assert_equal_i(client, s_write_head->client);
    cl_assert_equal_b(is_response_required, s_write_head->is_response_required);
  } else {
    fake_gatt_client_op_assert_write_failed();
  }
  Write *old_write = s_write_head;
  s_write_head = (Write *) list_pop_head(&s_write_head->node);
  free(old_write->value);
  free(old_write);
}
