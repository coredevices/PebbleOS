/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

void accessory_enable_input(void);
void accessory_disable_input(void);

void accessory_use_dma(bool use_dma);

void accessory_send_byte(uint8_t data);

typedef bool (*AccessoryDataStreamCallback)(void *context);
void accessory_send_stream(AccessoryDataStreamCallback callback, void *context);

void fake_accessory_get_buffer(uint8_t **buffer, int *length);
