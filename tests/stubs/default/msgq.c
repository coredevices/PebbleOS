/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/compiler.h>
#include <pbl/kernel/msgq.h>
#include <pbl/kernel/poll.h>

PBL_WEAK void pbl_msgq_init(struct pbl_msgq *q, void *buf, size_t msg_size, uint32_t max_msgs) {
}

PBL_WEAK void pbl_msgq_deinit(struct pbl_msgq *q) {
}

PBL_WEAK int pbl_msgq_put(struct pbl_msgq *q, const void *msg, pbl_timeout_t timeout) {
  return 0;
}

PBL_WEAK int pbl_msgq_put_front(struct pbl_msgq *q, const void *msg, pbl_timeout_t timeout) {
  return 0;
}

PBL_WEAK int pbl_msgq_get(struct pbl_msgq *q, void *msg, pbl_timeout_t timeout) {
  return 0;
}

PBL_WEAK int pbl_msgq_peek(struct pbl_msgq *q, void *msg) {
  return 0;
}

PBL_WEAK void pbl_msgq_purge(struct pbl_msgq *q) {
}

PBL_WEAK uint32_t pbl_msgq_num_used(const struct pbl_msgq *q) {
  return 0;
}

PBL_WEAK uint32_t pbl_msgq_num_free(const struct pbl_msgq *q) {
  return q->max_msgs;
}

PBL_WEAK void pbl_poll_group_init(struct pbl_poll_group *g) {
}

PBL_WEAK void pbl_poll_group_add(struct pbl_poll_group *g, struct pbl_msgq *q) {
}

PBL_WEAK struct pbl_msgq *pbl_poll_group_wait(struct pbl_poll_group *g, pbl_timeout_t timeout) {
  return nullptr;
}

PBL_WEAK bool pbl_poll_group_is_empty(const struct pbl_poll_group *g) {
  return true;
}
