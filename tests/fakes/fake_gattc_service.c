/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "fake_gattc_service.h"

#include <errno.h>

static struct pbl_bt_gattc_service *s_svc;
static const struct pbl_bt_gattc_service_cb *s_cb;
static FakeGattcServiceWriteHandler s_write_handler;
static int s_subscribe_rc;
static int s_read_rc;
static int s_num_reads;

void fake_gattc_service_init(void) {
  s_svc = nullptr;
  s_cb = nullptr;
  s_write_handler = nullptr;
  s_subscribe_rc = 0;
  s_read_rc = 0;
  s_num_reads = 0;
}

void fake_gattc_service_set_write_handler(FakeGattcServiceWriteHandler handler) {
  s_write_handler = handler;
}

void fake_gattc_service_set_subscribe_rc(int rc) {
  s_subscribe_rc = rc;
}

void fake_gattc_service_set_read_rc(int rc) {
  s_read_rc = rc;
}

int fake_gattc_service_num_reads(void) {
  return s_num_reads;
}

bool fake_gattc_service_is_started(void) {
  return s_svc != nullptr;
}

void fake_gattc_service_discovered(int status) {
  if (s_svc != nullptr && s_cb->discovered != nullptr) {
    s_cb->discovered(s_svc, status);
  }
}

void fake_gattc_service_subscribed(uint8_t chr, bool enable, int status) {
  if (s_svc != nullptr && s_cb->subscribed != nullptr) {
    s_cb->subscribed(s_svc, chr, enable, status);
  }
}

void fake_gattc_service_notified(uint8_t chr, const void *data, uint16_t len) {
  if (s_svc != nullptr && s_cb->notified != nullptr) {
    s_cb->notified(s_svc, chr, data, len);
  }
}

void fake_gattc_service_written(uint8_t chr, int status) {
  if (s_svc != nullptr && s_cb->written != nullptr) {
    s_cb->written(s_svc, chr, status);
  }
}

void fake_gattc_service_read(uint8_t chr, int status, const void *data, uint16_t len) {
  if (s_svc != nullptr && s_cb->read != nullptr) {
    s_cb->read(s_svc, chr, status, data, len);
  }
}

int pbl_bt_gattc_service_start(struct pbl_bt_gattc_service *svc, uint16_t conn_handle,
                               const struct pbl_bt_gattc_service_cb *cb) {
  s_svc = svc;
  s_cb = cb;
  return 0;
}

int pbl_bt_gattc_service_rediscover(struct pbl_bt_gattc_service *svc) {
  return (s_svc == svc) ? 0 : -ENOTCONN;
}

void pbl_bt_gattc_service_stop(struct pbl_bt_gattc_service *svc) {
  if (s_svc == svc) {
    s_svc = nullptr;
  }
}

int pbl_bt_gattc_service_subscribe(struct pbl_bt_gattc_service *svc, uint8_t chr, bool enable) {
  return (s_svc == svc) ? s_subscribe_rc : -ENOTCONN;
}

int pbl_bt_gattc_service_write(struct pbl_bt_gattc_service *svc, uint8_t chr, const void *data,
                               uint16_t len) {
  if (s_svc != svc) {
    return -ENOTCONN;
  }
  return (s_write_handler != nullptr) ? s_write_handler(chr, data, len) : 0;
}

int pbl_bt_gattc_service_read(struct pbl_bt_gattc_service *svc, uint8_t chr) {
  if (s_svc != svc) {
    return -ENOTCONN;
  }
  s_num_reads++;
  return s_read_rc;
}

bool pbl_bt_gattc_service_notify_rx(struct pbl_bt_gattc_service *svc, uint16_t conn_handle,
                                    uint16_t attr_handle, struct os_mbuf *om) {
  return false;
}
