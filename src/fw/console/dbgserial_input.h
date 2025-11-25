/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>


//! @file dbgserial_input.h
//! Contains the input related functionality of the debug serial port.

//! Initializes the input portions of the dbgserial driver.
void dbgserial_input_init(void);

typedef void (*DbgSerialCharacterCallback)(char c, bool* should_context_switch);
void dbgserial_register_character_callback(DbgSerialCharacterCallback callback);

void dbgserial_enable_rx_exti(void);

//! Enables/disables DMA-based receiving
void dbgserial_set_rx_dma_enabled(bool enabled);

#if MICRO_FAMILY_NRF5
void dbgserial_disable_rx_dma_before_stop();
void dbgserial_enable_rx_dma_after_stop();
#endif
