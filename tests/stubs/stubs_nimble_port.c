/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

/* What the inline functions of the NimBLE headers refer to; none is called. */

#define STUB(_name)  \
  void _name(void) { \
  }

STUB(ble_transport_to_hs_acl_impl)
STUB(ble_transport_to_hs_evt_impl)
STUB(ble_transport_to_hs_iso_impl)
STUB(ble_transport_to_ll_acl_impl)
STUB(ble_transport_to_ll_cmd_impl)
STUB(ble_transport_to_ll_iso_impl)
STUB(npl_pebble_callout_get_ticks)
STUB(npl_pebble_callout_init)
STUB(npl_pebble_callout_is_active)
STUB(npl_pebble_callout_remaining_ticks)
STUB(npl_pebble_callout_reset)
STUB(npl_pebble_callout_stop)
STUB(npl_pebble_eventq_get)
STUB(npl_pebble_eventq_put)
STUB(npl_pebble_eventq_remove)
STUB(npl_pebble_mutex_init)
STUB(npl_pebble_mutex_pend)
STUB(npl_pebble_mutex_release)
STUB(npl_pebble_sem_init)
STUB(npl_pebble_sem_pend)
STUB(npl_pebble_sem_release)
STUB(npl_pebble_time_ms_to_ticks)
STUB(npl_pebble_time_ticks_to_ms)
STUB(pbl_kernel_is_started)
