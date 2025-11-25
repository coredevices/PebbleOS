/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "fake_accessory.h"

#include "clar_asserts.h"

#include "services/normal/accessory/smartstrap_comms.h"

#include <string.h>

#define BUFFER_LENGTH 200

static uint8_t s_buffer[BUFFER_LENGTH];
static int s_buffer_index = 0;
static bool s_did_send_byte = false;

void accessory_disable_input(void) {
}

void accessory_enable_input(void) {
}

void accessory_use_dma(bool use_dma) {
}

void accessory_send_byte(uint8_t data) {
  cl_assert(s_buffer_index < BUFFER_LENGTH);
  s_buffer[s_buffer_index++] = data;
  s_did_send_byte = true;
}

void accessory_send_stream(AccessoryDataStreamCallback callback, void *context) {
  s_buffer_index = 0;
  memset(s_buffer, 0, BUFFER_LENGTH);
  while (callback(context)) {
    cl_assert(s_did_send_byte);
  }
}

void fake_accessory_get_buffer(uint8_t **buffer, int *length) {
  *buffer = s_buffer;
  *length = s_buffer_index;
}
