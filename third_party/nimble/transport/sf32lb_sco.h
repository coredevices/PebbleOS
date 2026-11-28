/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

struct ble_hci_ev;

// The controller keeps the audio of a synchronous connection off HCI, in a pair of rings of its
// own. These turn it into HCI synchronous data and back, on the HCI thread.

//! Take the audio doorbell and set the rings up, once the LCPU runs
void nimble_hci__sf32lb_sco_init(void);

//! Follow the synchronous connection from the events of the controller
void nimble_hci__sf32lb_sco_event(const struct ble_hci_ev *ev);

//! Hand the audio received since the last call to the host
void nimble_hci__sf32lb_sco_poll(void);

//! Wake the HCI thread
void nimble_hci__sf32lb_wake(void);
