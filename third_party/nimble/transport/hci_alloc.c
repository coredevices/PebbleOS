/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "hci_alloc.h"

#include <pbl/kernel/sem.h>
#include <pbl/kernel/thread.h>

// clang-format off
#include <os/os_mbuf.h>
// clang-format on
#include <nimble/transport.h>
#include <os/os_mempool.h>

#define ACL_POOL_WAIT_MS 100U
#define EVT_POOL_POLL_MS 5U

// Given when the host frees an ACL buffer of the controller's.
static PBL_SEM_DEFINE(s_acl_pool_avail, 0, 1);

static struct os_mbuf *prv_alloc_acl(void) {
  struct os_mbuf *om;

  // The timeout covers a buffer freed before the wait, or taken by the host's side of the pool.
  while ((om = ble_transport_alloc_acl_from_ll()) == NULL) {
    (void)pbl_sem_take(&s_acl_pool_avail, PBL_MSEC(ACL_POOL_WAIT_MS));
  }

  return om;
}

static os_error_t prv_acl_put(struct os_mempool_ext *mpe, void *data, void *arg) {
  os_error_t err = os_memblock_put_from_cb(&mpe->mpe_mp, data);

  pbl_sem_give(&s_acl_pool_avail);

  return err;
}

static void *prv_alloc_evt(int discardable) {
  void *buf;

  // A discardable event without a buffer is dropped by the state machine.
  while ((buf = ble_transport_alloc_evt(discardable)) == NULL && !discardable) {
    pbl_thread_sleep(PBL_MSEC(EVT_POOL_POLL_MS));
  }

  return buf;
}

const struct hci_h4_allocators nimble_hci__allocs_from_ll = {
  .acl = prv_alloc_acl,
  .evt = prv_alloc_evt,
};

void nimble_hci__alloc_init(void) {
  ble_transport_register_put_acl_from_ll_cb(prv_acl_put);
}
