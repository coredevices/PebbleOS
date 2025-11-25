/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// If a ping is due to be sent, send it. This should be called when we are already sending other
// data to the phone anyways in order to minimize the number of times we have to wake up the phone.
// It will return without doing anything if a minimum amount of time (currently 1 hour)
// has not elapsed since the last ping was sent out.
void ping_send_if_due(void);
