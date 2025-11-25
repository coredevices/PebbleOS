/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/app_smartstrap.h"
#include "services/normal/accessory/smartstrap_profiles.h"
#include "util/mbuf.h"

#include <stdbool.h>
#include <stdint.h>

#define SMARTSTRAP_PROTOCOL_VERSION 1

//! Initialize the smartstrap manager
void smartstrap_comms_init(void);

//! Called by accessory_manager when we receive a byte of data from the accessory port
bool smartstrap_handle_data_from_isr(uint8_t c);

//! Called by accessory_manager when we receive a break character
bool smartstrap_handle_break_from_isr(void);

//! Sends a message over the accessory port using the smartstrap protocol. The message will be sent
//! synchronously and the response will be read asynchronously with an event being put on the
//! calling task's queue when the response is read or a timeout occurs. A response will only be
//! expected if read_data is non-NULL.
//! @note The calling task must be subscribed first (@see sys_smartstrap_subscribe)
//! @param[in] profile The profile of the frame
//! @param[in] write_data The data to be written to the smartstrap
//! @param[in] write_length The length of write_data
//! @param[in] read_data The buffer to store the response in (asynchronously)
//! @param[in] read_length The length of the read_data buffer
//! @param[in] timeout_ms A timeout will occur if the response is not received after this amount of
//! time
//! @return The result of the send
SmartstrapResult smartstrap_send(SmartstrapProfile profile, MBuf *write_mbuf, MBuf *read_mbuf,
                                 uint16_t timeout_ms);


//! Enables or disables the smartstrap communications
void smartstrap_comms_set_enabled(bool enabled);

//! Cancels any send (write or read) which is in progress
void smartstrap_cancel_send(void);
