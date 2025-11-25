/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

//! Retransmit timer for PULSEv2 Reliable Transport

#pragma once

#include <stdint.h>

//! Start or restart the PULSEv2 reliable transport retransmit timer.
void pulse2_reliable_retransmit_timer_start(
    unsigned int timeout_ms, uint8_t sequence_number);

//! Cancel a running retransmit timer if it has not already expired.
//!
//! It is a no-op to call this function when the timer is already stopped.
void pulse2_reliable_retransmit_timer_cancel(void);

//! The function which is called when the retransmit timer expires.
//!
//! It is executed from the context of the PULSE task.
void pulse2_reliable_retransmit_timer_expired_handler(uint8_t sequence_number);
