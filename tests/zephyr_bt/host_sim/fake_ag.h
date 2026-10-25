/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

//! Sends an H4 packet (type byte first) from the controller to the host.
typedef void (*fake_ag_send_t)(const uint8_t *h4, size_t len);

struct fake_ag_state {
  //! The watch set the service level connection up (AT+CMER seen).
  bool slc;
  bool call;
  //! The voice link is up.
  bool sco;
  uint32_t sdp_requests;
  uint32_t answers;
  uint32_t hangups;
};

#define FAKE_AG_SCO_HANDLE 0x0101

void fake_ag_init(fake_ag_send_t send, uint16_t handle, const uint8_t addr[6]);

//! The watch accepted the voice link (Accept Synchronous Connection Request).
void fake_ag_sco_accepted(void);

//! A new ACL link to the watch.
void fake_ag_reset(void);

//! The ACL link goes down: the voice link with it.
void fake_ag_link_down(void);

//! An ACL packet (from its header on) the watch sent to the phone.
void fake_ag_acl(const uint8_t *acl, size_t len);

//! The phone's own number (AT+CNUM).
#define FAKE_AG_OWN_NUMBER "+819011112222"

void fake_ag_incoming_call(const char *number);
//! A call dialed on the phone: the phone opens the voice link, the other side answers.
void fake_ag_outgoing_call(void);
//! A call dialed on the phone, ringing at the other side until fake_ag_remote_answer().
void fake_ag_outgoing_ringing(void);
void fake_ag_remote_answer(void);
//! The ringing call answered on the phone.
void fake_ag_answer_on_phone(void);
//! The phone stops listing its call (AT+CLCC), without saying so.
void fake_ag_forget_call(void);
//! The phone closes the voice link.
void fake_ag_close_sco(void);
//! The watch closed the voice link.
void fake_ag_sco_closed(void);

const struct fake_ag_state *fake_ag_state(void);
