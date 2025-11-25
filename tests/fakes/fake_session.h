/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "services/common/comm_session/session_transport.h"

//
// Usage hints:
// ------------
//
// Typically, you'll want to do something like:
//
// 1. Connect a fake transport for the system CommSession:
//
//    Transport *transport = fake_transport_create(TransportDestinationSystem, NULL, NULL);
//    fake_transport_set_connected(transport, true /* connected */);
//
// 2. Simulate receiving some data by calling your module's endpoint handler:
//
//    put_bytes_protocol_msg_callback(comm_session_get_system_session(), msg, sizeof(msg));
//
// 3. Process the outbound data that has been queued up by your endpoint implementation:
//
//    fake_comm_session_process_send_next();
//
// 4. Assert the sent data is what you expect:
//
//    const uint8_t expected_payload[] = { 0x01, 0x02, 0x03 };
//    fake_transport_assert_sent(transport, 0, endpoint_id,
//                               expected_payload, sizeof(expected_payload));

////////////////////////////////////////////////////////////////////////////////////////////////////
// Session related functions

ResponsivenessGrantedHandler fake_comm_session_get_last_responsiveness_granted_handler(void);

int fake_comm_session_open_call_count(void);
int fake_comm_session_close_call_count(void);

void fake_comm_session_process_send_next(void);

uint32_t fake_comm_session_get_responsiveness_max_period(void);
uint32_t fake_comm_session_is_latency_reduced(void);

////////////////////////////////////////////////////////////////////////////////////////////////////
// Transport mock

//! Pointer to function handling data that is sent out by the session
typedef void (*FakeTransportSentCallback)(uint16_t endpoint_id,
                                          const uint8_t* data, unsigned int data_length);

//! Creates a mock transport
//! @param destination The destination type this transport is connected to. See comments with the
//! TransportDestination struct for more info.
//! @param app_uuid The UUID of the app that this transport is connected to.
//! Pass NULL if this information is not known or irrelevant.
//! @param sent_cb The callback that needs to be called whenever data is sent out using this mock
//! transport. Note that data will only be sent out when fake_comm_session_process_send_next() is
//! called. It's recommended to leave this NULL and use fake_transport_assert_sent instead.
Transport *fake_transport_create(TransportDestination destination,
                                 const Uuid *app_uuid,
                                 FakeTransportSentCallback sent_cb);

//! Simulating (dis)connecting the transport.
//! @return When connected, returns the opened CommSession. Returns NULL when disconnected.
CommSession *fake_transport_set_connected(Transport *transport, bool connected);

//! Asserts the data of sent packets.
//! @note This function can only be used when fake_transport_set_sent_cb is not used. They are
//! mutually exclusive.
//! @param index Packet index. Zero-based, newest packet first, oldest last.
void fake_transport_assert_sent(Transport *transport, uint16_t index, uint16_t endpoint_id,
                                const uint8_t data[], size_t length);

//! Asserts no data has been sent out.
void fake_transport_assert_nothing_sent(Transport *transport);

//! Assigns a new callback that needs to be called whenever data is sent out using this mock
//! transport. Note that data will only be sent out when fake_comm_session_process_send_next() is
//! called. It's recommended to use fake_transport_assert_sent, because it results in tests that
//! are much easier to read.
void fake_transport_set_sent_cb(Transport *transport, FakeTransportSentCallback sent_cb);

//! Destroys the mock transport 
void fake_transport_destroy(Transport *transport);

////////////////////////////////////////////////////////////////////////////////////////////////////
// Transport helper functions

//! Writes data into the fake send buffer, skipping Pebble Protocol
//! @return false if there's insufficient space.
bool fake_comm_session_send_buffer_write_raw_by_transport(Transport *transport,
                                                          const uint8_t *data, size_t length);

////////////////////////////////////////////////////////////////////////////////////////////////////
// Fake life cycle

void fake_comm_session_init(void);
void fake_comm_session_cleanup(void);
