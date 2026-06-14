/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

// GENERATED -- DO NOT EDIT

#include "test_endpoint_ids.h"

extern void private_test_protocol_msg_callback(CommSession *session,
                                               const uint8_t* data, size_t length);
extern void public_test_protocol_msg_callback(CommSession *session,
                                              const uint8_t* data, size_t length);
extern void any_test_protocol_msg_callback(CommSession *session,
                                           const uint8_t* data, size_t length);

extern ReceiverImplementation g_system_test_receiver_imp;

static const PebbleProtocolEndpoint s_protocol_endpoints[] = {
  { PRIVATE_TEST_ENDPOINT_ID, private_test_protocol_msg_callback,
    PebbleProtocolAccessPrivate, &g_system_test_receiver_imp },
  { PUBLIC_TEST_ENDPOINT_ID, public_test_protocol_msg_callback,
    PebbleProtocolAccessPublic, &g_system_test_receiver_imp },
  { ANY_TEST_ENDPOINT_ID, any_test_protocol_msg_callback,
    PebbleProtocolAccessAny, &g_system_test_receiver_imp },
};

