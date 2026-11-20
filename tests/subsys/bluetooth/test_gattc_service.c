/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "bluetooth/clients/op_queue.h"

#include <errno.h>
#include <string.h>

#include <pbl/bluetooth/clients/gatt.h>
#include <pbl/kernel/malloc.h>
#include <pbl/util/bits.h>
#include <pbl/util/size.h>

#include <clar.h>
#include <host/ble_hs.h>

#define CONN_HANDLE 1

enum {
  CHR_A,
  CHR_B,
};

static const ble_uuid16_t s_svc_uuid = BLE_UUID16_INIT(0x1234);
static const ble_uuid16_t s_chr_a_uuid = BLE_UUID16_INIT(0x1235);
static const ble_uuid16_t s_chr_b_uuid = BLE_UUID16_INIT(0x1236);
static const ble_uuid_t *const s_chr_uuids[] = {&s_chr_a_uuid.u, &s_chr_b_uuid.u};

PBL_BT_GATTC_SERVICE_DEFINE(s_svc, &s_svc_uuid.u, s_chr_uuids, 0, PBL_BIT(CHR_A));

/* The discoveries that wait, oldest first. */
static struct {
  NimbleGattClientOpStartFn start;
  void *ctx;
} s_ops[4];
static uint8_t s_num_ops;

static ble_gatt_disc_svc_fn *s_svc_cb;
static ble_gatt_chr_fn *s_chr_cb;
static ble_gatt_dsc_fn *s_dsc_cb;
static void *s_disc_arg;

/* The writes that await their answer, oldest first. */
static struct {
  uint16_t handle;
  ble_gatt_attr_fn *cb;
  void *arg;
} s_writes[4];
static uint8_t s_num_writes;

static int s_num_discovered;
static int s_discovered_status;
static int s_num_subscribed;
static int s_num_written;

void nimble_gattc_op_queue_push(NimbleGattClientOpStartFn start, void *ctx) {
  cl_assert(s_num_ops < PBL_ARRAY_SIZE(s_ops));
  s_ops[s_num_ops].start = start;
  s_ops[s_num_ops].ctx = ctx;
  s_num_ops++;
}

void nimble_gattc_op_queue_complete(void) {
}

int ble_gattc_disc_svc_by_uuid(uint16_t conn_handle, const ble_uuid_t *uuid,
                               ble_gatt_disc_svc_fn *cb, void *cb_arg) {
  s_svc_cb = cb;
  s_disc_arg = cb_arg;
  return 0;
}

int ble_gattc_disc_all_chrs(uint16_t conn_handle, uint16_t start_handle, uint16_t end_handle,
                            ble_gatt_chr_fn *cb, void *cb_arg) {
  s_chr_cb = cb;
  return 0;
}

int ble_gattc_disc_all_dscs(uint16_t conn_handle, uint16_t start_handle, uint16_t end_handle,
                            ble_gatt_dsc_fn *cb, void *cb_arg) {
  s_dsc_cb = cb;
  return 0;
}

int ble_gattc_write_flat(uint16_t conn_handle, uint16_t attr_handle, const void *data,
                         uint16_t data_len, ble_gatt_attr_fn *cb, void *cb_arg) {
  cl_assert(s_num_writes < PBL_ARRAY_SIZE(s_writes));
  s_writes[s_num_writes].handle = attr_handle;
  s_writes[s_num_writes].cb = cb;
  s_writes[s_num_writes].arg = cb_arg;
  s_num_writes++;
  return 0;
}

int ble_gattc_write_no_rsp_flat(uint16_t conn_handle, uint16_t attr_handle, const void *data,
                                uint16_t data_len) {
  return 0;
}

int ble_gattc_read_long(uint16_t conn_handle, uint16_t handle, uint16_t offset,
                        ble_gatt_attr_fn *cb, void *cb_arg) {
  return BLE_HS_ENOTSUP;
}

int ble_uuid_cmp(const ble_uuid_t *uuid1, const ble_uuid_t *uuid2) {
  return BLE_UUID16(uuid1)->value != BLE_UUID16(uuid2)->value;
}

int os_mbuf_copydata(const struct os_mbuf *om, int off, int len, void *dst) {
  return -1;
}

/* Runs the oldest discovery: the service is at the handle given, or is not there if it is 0. */
static void prv_run_discovery(uint16_t start_handle) {
  const struct ble_gatt_error ok = {};
  const struct ble_gatt_error done = {.status = BLE_HS_EDONE};
  void *ctx;

  cl_assert(s_num_ops > 0);
  ctx = s_ops[0].ctx;
  cl_assert_equal_i(s_ops[0].start(ctx), 0);
  s_num_ops--;
  memmove(&s_ops[0], &s_ops[1], s_num_ops * sizeof(s_ops[0]));

  if (start_handle != 0) {
    const struct ble_gatt_svc svc = {
      .start_handle = start_handle,
      .end_handle = start_handle + 5,
      .uuid.u16 = s_svc_uuid,
    };
    const struct ble_gatt_chr chr_a = {
      .def_handle = start_handle + 1,
      .val_handle = start_handle + 2,
      .properties = BLE_GATT_CHR_PROP_NOTIFY,
      .uuid.u16 = s_chr_a_uuid,
    };
    const struct ble_gatt_chr chr_b = {
      .def_handle = start_handle + 4,
      .val_handle = start_handle + 5,
      .properties = BLE_GATT_CHR_PROP_WRITE,
      .uuid.u16 = s_chr_b_uuid,
    };
    const struct ble_gatt_dsc cccd = {
      .handle = start_handle + 3,
      .uuid.u16 = BLE_UUID16_INIT(BLE_GATT_DSC_CLT_CFG_UUID16),
    };

    s_svc_cb(CONN_HANDLE, &ok, &svc, s_disc_arg);
    s_svc_cb(CONN_HANDLE, &done, nullptr, s_disc_arg);
    s_chr_cb(CONN_HANDLE, &ok, &chr_a, s_disc_arg);
    s_chr_cb(CONN_HANDLE, &ok, &chr_b, s_disc_arg);
    s_chr_cb(CONN_HANDLE, &done, nullptr, s_disc_arg);
    s_dsc_cb(CONN_HANDLE, &ok, chr_a.val_handle, &cccd, s_disc_arg);
    s_dsc_cb(CONN_HANDLE, &done, 0, nullptr, s_disc_arg);
  } else {
    s_svc_cb(CONN_HANDLE, &done, nullptr, s_disc_arg);
  }

  pbl_free(ctx);
}

/* Answers the oldest write and tells the handle it was made on. */
static uint16_t prv_answer_write(void) {
  const struct ble_gatt_error ok = {};
  const uint16_t handle = s_writes[0].handle;
  ble_gatt_attr_fn *cb = s_writes[0].cb;
  void *arg = s_writes[0].arg;

  cl_assert(s_num_writes > 0);
  s_num_writes--;
  memmove(&s_writes[0], &s_writes[1], s_num_writes * sizeof(s_writes[0]));
  cb(CONN_HANDLE, &ok, nullptr, arg);

  return handle;
}

static void prv_discovered(struct pbl_bt_gattc_service *svc, int status) {
  s_num_discovered++;
  s_discovered_status = status;
}

static void prv_subscribed(struct pbl_bt_gattc_service *svc, uint8_t chr, bool enable, int status) {
  s_num_subscribed++;
}

static void prv_written(struct pbl_bt_gattc_service *svc, uint8_t chr, int status) {
  s_num_written++;
}

static const struct pbl_bt_gattc_service_cb s_cb = {
  .discovered = prv_discovered,
  .subscribed = prv_subscribed,
  .written = prv_written,
};

void test_gattc_service__initialize(void) {
  s_num_ops = 0;
  s_num_writes = 0;
  s_num_discovered = 0;
  s_discovered_status = 0;
  s_num_subscribed = 0;
  s_num_written = 0;

  cl_assert_equal_i(pbl_bt_gattc_service_start(&s_svc, CONN_HANDLE, &s_cb), 0);
  prv_run_discovery(0x10);
  cl_assert_equal_i(s_num_discovered, 1);
  cl_assert_equal_i(s_discovered_status, 0);
}

void test_gattc_service__cleanup(void) {
  pbl_bt_gattc_service_stop(&s_svc);
  while (s_num_writes > 0) {
    prv_answer_write();
  }
  while (s_num_ops > 0) {
    prv_run_discovery(0);
  }
}

void test_gattc_service__answers(void) {
  cl_assert_equal_i(pbl_bt_gattc_service_subscribe(&s_svc, CHR_A, true), 0);
  cl_assert_equal_i(pbl_bt_gattc_service_write(&s_svc, CHR_B, "x", 1), 0);

  cl_assert_equal_i(prv_answer_write(), 0x13);
  cl_assert_equal_i(s_num_subscribed, 1);
  cl_assert_equal_i(prv_answer_write(), 0x15);
  cl_assert_equal_i(s_num_written, 1);
}

void test_gattc_service__rediscover_where_it_was_keeps_answers(void) {
  cl_assert_equal_i(pbl_bt_gattc_service_write(&s_svc, CHR_B, "x", 1), 0);

  cl_assert_equal_i(pbl_bt_gattc_service_rediscover(&s_svc), 0);
  prv_run_discovery(0x10);
  cl_assert_equal_i(s_num_discovered, 1);

  prv_answer_write();
  cl_assert_equal_i(s_num_written, 1);
}

void test_gattc_service__rediscover_moved_drops_answers(void) {
  cl_assert_equal_i(pbl_bt_gattc_service_subscribe(&s_svc, CHR_A, true), 0);
  cl_assert_equal_i(pbl_bt_gattc_service_write(&s_svc, CHR_B, "x", 1), 0);

  cl_assert_equal_i(pbl_bt_gattc_service_rediscover(&s_svc), 0);
  prv_run_discovery(0x20);
  cl_assert_equal_i(s_num_discovered, 2);
  cl_assert_equal_i(s_discovered_status, 0);

  /* What was asked of the service where it was. */
  prv_answer_write();
  prv_answer_write();
  cl_assert_equal_i(s_num_subscribed, 0);
  cl_assert_equal_i(s_num_written, 0);

  /* What is asked of it where it is now. */
  cl_assert_equal_i(pbl_bt_gattc_service_write(&s_svc, CHR_B, "x", 1), 0);
  cl_assert_equal_i(prv_answer_write(), 0x25);
  cl_assert_equal_i(s_num_written, 1);
}

void test_gattc_service__rediscover_gone_drops_answers(void) {
  cl_assert_equal_i(pbl_bt_gattc_service_write(&s_svc, CHR_B, "x", 1), 0);

  cl_assert_equal_i(pbl_bt_gattc_service_rediscover(&s_svc), 0);
  prv_run_discovery(0);
  cl_assert_equal_i(s_num_discovered, 2);
  cl_assert_equal_i(s_discovered_status, -ENOENT);

  prv_answer_write();
  cl_assert_equal_i(s_num_written, 0);
  cl_assert_equal_i(pbl_bt_gattc_service_write(&s_svc, CHR_B, "x", 1), -ENOTCONN);
}

void test_gattc_service__rediscover_twice_reports_both(void) {
  cl_assert_equal_i(pbl_bt_gattc_service_rediscover(&s_svc), 0);
  cl_assert_equal_i(pbl_bt_gattc_service_rediscover(&s_svc), 0);

  prv_run_discovery(0x20);
  cl_assert_equal_i(s_num_discovered, 2);

  prv_run_discovery(0x30);
  cl_assert_equal_i(s_num_discovered, 3);

  cl_assert_equal_i(pbl_bt_gattc_service_write(&s_svc, CHR_B, "x", 1), 0);
  cl_assert_equal_i(prv_answer_write(), 0x35);
}

void test_gattc_service__rediscovery_from_before_a_restart_is_dropped(void) {
  cl_assert_equal_i(pbl_bt_gattc_service_rediscover(&s_svc), 0);

  pbl_bt_gattc_service_stop(&s_svc);
  cl_assert_equal_i(pbl_bt_gattc_service_start(&s_svc, CONN_HANDLE, &s_cb), 0);

  prv_run_discovery(0x20);
  cl_assert_equal_i(s_num_discovered, 1);

  prv_run_discovery(0x10);
  cl_assert_equal_i(s_num_discovered, 2);
  cl_assert_equal_i(pbl_bt_gattc_service_write(&s_svc, CHR_B, "x", 1), 0);
  cl_assert_equal_i(prv_answer_write(), 0x15);
}
