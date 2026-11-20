/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <string.h>

#include <pbl/bluetooth/clients/ancs.h>
#include <pbl/util/size.h>

#include <clar.h>
#include <fake_gattc_service.h>
#include <fake_pbl_malloc.h>

enum {
  CHR_NOTIFICATION_SOURCE,
  CHR_DATA_SOURCE,
  CHR_CONTROL_POINT,
};

#define UID 0x04030201

PBL_BT_ANCS_CLIENT_DEFINE(s_client, 64);

static const struct pbl_bt_ancs_attr_req s_attr_reqs[] = {
  {.id = PBL_BT_ANCS_NOTIF_ATTR_POSITIVE_ACTION_LABEL, .optional = true},
  {.id = PBL_BT_ANCS_NOTIF_ATTR_APP_IDENTIFIER},
  {.id = PBL_BT_ANCS_NOTIF_ATTR_TITLE, .max_len = 4},
  {.id = PBL_BT_ANCS_NOTIF_ATTR_DATE},
};

/* Get Notification Attributes for s_attr_reqs. */
static const uint8_t s_notif_cmd[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x06,
                                      0x00, 0x01, 0x04, 0x00, 0x05};

/* Its response, without the optional attribute. */
static const uint8_t s_notif_rsp[] = {
  0x00,
  0x01,
  0x02,
  0x03,
  0x04,
  /* app identifier */ 0x00,
  0x03,
  0x00,
  'a',
  '.',
  'b',
  /* title */ 0x01,
  0x02,
  0x00,
  'H',
  'i',
  /* date */ 0x05,
  0x01,
  0x00,
  '2',
};

static const struct pbl_bt_ancs_attr_req s_app_attr_reqs[] = {
  {.id = PBL_BT_ANCS_APP_ATTR_DISPLAY_NAME},
};

static const uint8_t s_app_cmd[] = {0x01, 'a', '.', 'b', 0x00, 0x00};
static const uint8_t s_app_rsp[] = {0x01, 'a', '.', 'b', 0x00, 0x00, 0x02, 0x00, 'A', 'B'};

static uint8_t s_written[32];
static uint16_t s_written_len;
static int s_num_writes;
static int s_write_rc;

static int s_num_discovered;
static int s_num_subscribed;
static int s_num_notifications;
static struct pbl_bt_ancs_notif s_notif;
static int s_num_results;
static int s_status;
static uint32_t s_uid;
static uint8_t s_num_attrs;
/* The lengths of the attributes got, -1 for the ones lacking, and their values in a row. */
static int s_attr_lens[PBL_BT_ANCS_CLIENT_ATTRS_MAX];
static char s_attr_values[64];

static int prv_handle_write(uint8_t chr, const uint8_t *data, uint16_t len) {
  cl_assert_equal_i(chr, CHR_CONTROL_POINT);
  cl_assert(len <= sizeof(s_written));
  memcpy(s_written, data, len);
  s_written_len = len;
  s_num_writes++;
  return s_write_rc;
}

static void prv_discovered(struct pbl_bt_ancs_client *client, int status) {
  cl_assert(client == &s_client);
  s_num_discovered++;
  s_status = status;
}

static void prv_subscribed(struct pbl_bt_ancs_client *client, enum pbl_bt_ancs_source source,
                           bool enable, int status) {
  cl_assert_equal_i(source, PBL_BT_ANCS_SOURCE_DATA);
  cl_assert(enable);
  s_num_subscribed++;
  s_status = status;
}

static void prv_notification(struct pbl_bt_ancs_client *client,
                             const struct pbl_bt_ancs_notif *notif) {
  s_num_notifications++;
  s_notif = *notif;
}

static void prv_record(int status, const struct pbl_bt_ancs_attr *const *attrs, uint8_t num_attrs) {
  s_num_results++;
  s_status = status;
  s_num_attrs = num_attrs;
  s_attr_values[0] = '\0';
  for (uint8_t i = 0; i < num_attrs; i++) {
    s_attr_lens[i] = -1;
    if (attrs != nullptr && attrs[i] != nullptr) {
      s_attr_lens[i] = attrs[i]->len;
      strncat(s_attr_values, (const char *)attrs[i]->value, attrs[i]->len);
    }
  }
}

static void prv_notif_attrs(struct pbl_bt_ancs_client *client, int status, uint32_t uid,
                            const struct pbl_bt_ancs_attr *const *attrs, uint8_t num_attrs) {
  cl_assert_equal_i(status == 0, attrs != nullptr);
  s_uid = uid;
  prv_record(status, attrs, num_attrs);
}

static void prv_app_attrs(struct pbl_bt_ancs_client *client, int status,
                          const struct pbl_bt_ancs_attr *const *attrs, uint8_t num_attrs) {
  prv_record(status, attrs, num_attrs);
}

static void prv_action_performed(struct pbl_bt_ancs_client *client, int status, uint32_t uid) {
  s_uid = uid;
  prv_record(status, nullptr, 0);
}

static const struct pbl_bt_ancs_client_cb s_cb = {
  .discovered = prv_discovered,
  .subscribed = prv_subscribed,
  .notification = prv_notification,
  .notif_attrs = prv_notif_attrs,
  .app_attrs = prv_app_attrs,
  .action_performed = prv_action_performed,
};

static void prv_get_notif_attrs(void) {
  cl_assert_equal_i(
      pbl_bt_ancs_client_get_notif_attrs(&s_client, UID, s_attr_reqs, PBL_ARRAY_SIZE(s_attr_reqs)),
      0);
}

static void prv_ack(int status) {
  fake_gattc_service_written(CHR_CONTROL_POINT, status);
}

static void prv_data(const uint8_t *data, uint16_t len) {
  fake_gattc_service_notified(CHR_DATA_SOURCE, data, len);
}

static void prv_assert_notif_rsp(void) {
  cl_assert_equal_i(s_num_results, 1);
  cl_assert_equal_i(s_status, 0);
  cl_assert_equal_i(s_uid, UID);
  cl_assert_equal_i(s_num_attrs, PBL_ARRAY_SIZE(s_attr_reqs));
  cl_assert_equal_i(s_attr_lens[0], -1);
  cl_assert_equal_i(s_attr_lens[1], 3);
  cl_assert_equal_i(s_attr_lens[2], 2);
  cl_assert_equal_i(s_attr_lens[3], 1);
  cl_assert_equal_s(s_attr_values, "a.bHi2");
}

void test_ancs_client__initialize(void) {
  s_written_len = 0;
  s_num_writes = 0;
  s_write_rc = 0;
  s_num_discovered = 0;
  s_num_subscribed = 0;
  s_num_notifications = 0;
  s_num_results = 0;
  s_status = 0;
  s_uid = 0;

  fake_gattc_service_init();
  fake_gattc_service_set_write_handler(prv_handle_write);
  cl_assert_equal_i(pbl_bt_ancs_client_start(&s_client, 1, &s_cb), 0);
}

void test_ancs_client__cleanup(void) {
  pbl_bt_ancs_client_stop(&s_client);
}

void test_ancs_client__start_and_stop(void) {
  cl_assert_equal_i(pbl_bt_ancs_client_start(&s_client, 1, &s_cb), -EALREADY);

  fake_gattc_service_discovered(-ENOENT);
  cl_assert_equal_i(s_num_discovered, 1);
  cl_assert_equal_i(s_status, -ENOENT);

  cl_assert_equal_i(pbl_bt_ancs_client_subscribe(&s_client, PBL_BT_ANCS_SOURCE_DATA, true), 0);
  fake_gattc_service_subscribed(CHR_DATA_SOURCE, true, 0);
  cl_assert_equal_i(s_num_subscribed, 1);

  pbl_bt_ancs_client_stop(&s_client);
  cl_assert(!fake_gattc_service_is_started());
  cl_assert_equal_i(
      pbl_bt_ancs_client_get_notif_attrs(&s_client, UID, s_attr_reqs, PBL_ARRAY_SIZE(s_attr_reqs)),
      -ENOTCONN);
  cl_assert_equal_i(pbl_bt_ancs_client_start(&s_client, 2, &s_cb), 0);
}

void test_ancs_client__rediscover(void) {
  fake_gattc_service_discovered(0);
  cl_assert_equal_i(pbl_bt_ancs_client_rediscover(&s_client), 0);

  /* The service moved: the request made of it where it was is forgotten. */
  prv_get_notif_attrs();
  fake_gattc_service_discovered(0);
  cl_assert_equal_i(s_num_discovered, 2);
  prv_ack(0);
  prv_data(s_notif_rsp, sizeof(s_notif_rsp));
  cl_assert_equal_i(s_num_results, 0);

  prv_get_notif_attrs();
  prv_ack(0);
  prv_data(s_notif_rsp, sizeof(s_notif_rsp));
  prv_assert_notif_rsp();

  pbl_bt_ancs_client_stop(&s_client);
  cl_assert_equal_i(pbl_bt_ancs_client_rediscover(&s_client), -ENOTCONN);
}

void test_ancs_client__notification_source(void) {
  static const uint8_t ns[] = {0x01, 0x05, 0x04, 0x07, 0x01, 0x02, 0x03, 0x04};

  fake_gattc_service_notified(CHR_NOTIFICATION_SOURCE, ns, sizeof(ns));
  cl_assert_equal_i(s_num_notifications, 1);
  cl_assert_equal_i(s_notif.event, PBL_BT_ANCS_EVENT_MODIFIED);
  cl_assert_equal_i(s_notif.flags, PBL_BT_ANCS_FLAG_SILENT | PBL_BT_ANCS_FLAG_PRE_EXISTING);
  cl_assert_equal_i(s_notif.category, PBL_BT_ANCS_CATEGORY_SOCIAL);
  cl_assert_equal_i(s_notif.category_count, 7);
  cl_assert_equal_i(s_notif.uid, UID);

  fake_gattc_service_notified(CHR_NOTIFICATION_SOURCE, ns, sizeof(ns) - 1);
  cl_assert_equal_i(s_num_notifications, 1);
}

void test_ancs_client__notif_attrs(void) {
  prv_get_notif_attrs();
  cl_assert_equal_i(s_written_len, sizeof(s_notif_cmd));
  cl_assert_equal_m(s_written, s_notif_cmd, sizeof(s_notif_cmd));

  prv_ack(0);
  cl_assert_equal_i(s_num_results, 0);
  prv_data(s_notif_rsp, sizeof(s_notif_rsp));
  prv_assert_notif_rsp();

  /* Nothing is in progress any more. */
  prv_data(s_notif_rsp, sizeof(s_notif_rsp));
  prv_ack(0);
  cl_assert_equal_i(s_num_results, 1);
}

void test_ancs_client__notif_attrs_before_ack(void) {
  prv_get_notif_attrs();
  prv_data(s_notif_rsp, sizeof(s_notif_rsp));
  cl_assert_equal_i(s_num_results, 0);
  prv_ack(0);
  prv_assert_notif_rsp();
}

void test_ancs_client__notif_attrs_in_parts(void) {
  prv_get_notif_attrs();
  prv_ack(0);
  for (size_t i = 0; i < sizeof(s_notif_rsp); i += 7) {
    cl_assert_equal_i(s_num_results, 0);
    prv_data(&s_notif_rsp[i], PBL_MIN(7, sizeof(s_notif_rsp) - i));
  }
  prv_assert_notif_rsp();
}

void test_ancs_client__notif_attrs_any_order_and_unknown(void) {
  static const uint8_t rsp[] = {
    0x00,
    0x01,
    0x02,
    0x03,
    0x04,
    /* date */ 0x05,
    0x01,
    0x00,
    '2',
    /* not asked for */ 0x04,
    0x01,
    0x00,
    '9',
    /* positive action */ 0x06,
    0x02,
    0x00,
    'O',
    'K',
    /* title */ 0x01,
    0x00,
    0x00,
    /* app identifier */ 0x00,
    0x01,
    0x00,
    'a',
  };

  prv_get_notif_attrs();
  prv_ack(0);
  prv_data(rsp, sizeof(rsp));
  cl_assert_equal_i(s_num_results, 1);
  cl_assert_equal_i(s_status, 0);
  cl_assert_equal_i(s_attr_lens[0], 2);
  cl_assert_equal_i(s_attr_lens[1], 1);
  cl_assert_equal_i(s_attr_lens[2], 0);
  cl_assert_equal_i(s_attr_lens[3], 1);
  cl_assert_equal_s(s_attr_values, "OKa2");
}

void test_ancs_client__value_too_long(void) {
  static const uint8_t rsp[] = {
    0x00, 0x01, 0x02, 0x03, 0x04, /* title */ 0x01, 0x05, 0x00, 'H',
  };

  prv_get_notif_attrs();
  prv_ack(0);
  prv_data(rsp, sizeof(rsp));
  cl_assert_equal_i(s_num_results, 1);
  cl_assert_equal_i(s_status, -EBADMSG);
}

void test_ancs_client__response_too_long(void) {
  uint8_t rsp[40] = {0x00, 0x01, 0x02, 0x03, 0x04, /* app identifier */ 0x00, 0xff, 0x00};

  prv_get_notif_attrs();
  prv_ack(0);
  prv_data(rsp, sizeof(rsp));
  cl_assert_equal_i(s_num_results, 0);
  prv_data(rsp, sizeof(rsp));
  cl_assert_equal_i(s_num_results, 1);
  cl_assert_equal_i(s_status, -ENOBUFS);
}

void test_ancs_client__response_to_something_else(void) {
  uint8_t rsp[sizeof(s_notif_rsp)];

  memcpy(rsp, s_notif_rsp, sizeof(rsp));
  rsp[1] ^= 0xff;

  prv_get_notif_attrs();
  prv_ack(0);
  prv_data(rsp, sizeof(rsp));
  cl_assert_equal_i(s_num_results, 1);
  cl_assert_equal_i(s_status, -EBADMSG);

  prv_get_notif_attrs();
  prv_ack(0);
  prv_data(s_app_rsp, sizeof(s_app_rsp));
  cl_assert_equal_i(s_num_results, 2);
  cl_assert_equal_i(s_status, -EBADMSG);
}

void test_ancs_client__one_request_at_a_time(void) {
  prv_get_notif_attrs();
  cl_assert_equal_i(
      pbl_bt_ancs_client_get_notif_attrs(&s_client, UID, s_attr_reqs, PBL_ARRAY_SIZE(s_attr_reqs)),
      -EBUSY);
  cl_assert_equal_i(pbl_bt_ancs_client_perform_action(&s_client, UID, PBL_BT_ANCS_ACTION_POSITIVE),
                    -EBUSY);
  cl_assert_equal_i(s_num_writes, 1);

  pbl_bt_ancs_client_cancel(&s_client);
  prv_ack(0);
  prv_data(s_notif_rsp, sizeof(s_notif_rsp));
  cl_assert_equal_i(s_num_results, 0);

  prv_get_notif_attrs();
  prv_ack(0);
  prv_data(s_notif_rsp, sizeof(s_notif_rsp));
  prv_assert_notif_rsp();
}

void test_ancs_client__request_refused(void) {
  s_write_rc = -EIO;
  cl_assert_equal_i(
      pbl_bt_ancs_client_get_notif_attrs(&s_client, UID, s_attr_reqs, PBL_ARRAY_SIZE(s_attr_reqs)),
      -EIO);
  cl_assert_equal_i(pbl_bt_ancs_client_get_notif_attrs(&s_client, UID, s_attr_reqs,
                                                       PBL_BT_ANCS_CLIENT_ATTRS_MAX + 1),
                    -EINVAL);

  s_write_rc = 0;
  prv_get_notif_attrs();
  prv_ack(PBL_BT_ANCS_ERR_INVALID_PARAMETER);
  cl_assert_equal_i(s_num_results, 1);
  cl_assert_equal_i(s_status, PBL_BT_ANCS_ERR_INVALID_PARAMETER);
  cl_assert_equal_i(s_uid, UID);
}

void test_ancs_client__no_attrs(void) {
  static const uint8_t cmd[] = {0x00, 0x01, 0x02, 0x03, 0x04};

  cl_assert_equal_i(pbl_bt_ancs_client_get_notif_attrs(&s_client, UID, nullptr, 0), 0);
  cl_assert_equal_i(s_written_len, sizeof(cmd));
  cl_assert_equal_m(s_written, cmd, sizeof(cmd));

  prv_ack(0);
  cl_assert_equal_i(s_num_results, 1);
  cl_assert_equal_i(s_status, 0);
  cl_assert_equal_i(s_num_attrs, 0);
}

void test_ancs_client__app_attrs(void) {
  cl_assert_equal_i(pbl_bt_ancs_client_get_app_attrs(&s_client, "a.b", 3, s_app_attr_reqs, 0),
                    -EINVAL);
  cl_assert_equal_i(pbl_bt_ancs_client_get_app_attrs(&s_client, "a.b", 3, s_app_attr_reqs,
                                                     PBL_ARRAY_SIZE(s_app_attr_reqs)),
                    0);
  cl_assert_equal_i(s_written_len, sizeof(s_app_cmd));
  cl_assert_equal_m(s_written, s_app_cmd, sizeof(s_app_cmd));

  prv_ack(0);
  prv_data(s_app_rsp, 3);
  prv_data(&s_app_rsp[3], sizeof(s_app_rsp) - 3);
  cl_assert_equal_i(s_num_results, 1);
  cl_assert_equal_i(s_status, 0);
  cl_assert_equal_i(s_num_attrs, 1);
  cl_assert_equal_i(s_attr_lens[0], 2);
  cl_assert_equal_s(s_attr_values, "AB");
}

void test_ancs_client__action(void) {
  static const uint8_t cmd[] = {0x02, 0x01, 0x02, 0x03, 0x04, 0x01};

  cl_assert_equal_i(pbl_bt_ancs_client_perform_action(&s_client, UID, PBL_BT_ANCS_ACTION_NEGATIVE),
                    0);
  cl_assert_equal_i(s_written_len, sizeof(cmd));
  cl_assert_equal_m(s_written, cmd, sizeof(cmd));

  prv_ack(PBL_BT_ANCS_ERR_ACTION_FAILED);
  cl_assert_equal_i(s_num_results, 1);
  cl_assert_equal_i(s_status, PBL_BT_ANCS_ERR_ACTION_FAILED);
  cl_assert_equal_i(s_uid, UID);

  cl_assert_equal_i(pbl_bt_ancs_client_perform_action(&s_client, UID, PBL_BT_ANCS_ACTION_POSITIVE),
                    0);
  prv_ack(0);
  cl_assert_equal_i(s_num_results, 2);
  cl_assert_equal_i(s_status, 0);
}
