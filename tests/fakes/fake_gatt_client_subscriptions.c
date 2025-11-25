/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "fake_gatt_client_subscriptions.h"

#include "clar_asserts.h"

typedef struct {
  ListNode node;
  BLECharacteristic characteristic;
  BLESubscription subscription_type;
  GAPLEClient client;
} Subscribe;

static Subscribe *s_subscribe_head;

static BTErrno s_subscribe_return_value;

BTErrno gatt_client_subscriptions_subscribe(BLECharacteristic characteristic,
                                            BLESubscription subscription_type,
                                            GAPLEClient client) {
  Subscribe *subscribe = malloc(sizeof(Subscribe));
  *subscribe = (const Subscribe) {
    .characteristic = characteristic,
    .subscription_type = subscription_type,
    .client = client,
  };
  if (s_subscribe_head) {
    list_append((ListNode *)s_subscribe_head, &subscribe->node);
  } else {
    s_subscribe_head = subscribe;
  }
  return s_subscribe_return_value;
}

bool gatt_client_subscriptions_get_notification_header(GAPLEClient client,
                                                       GATTBufferedNotificationHeader *header_out) {
  return false;
}

uint16_t gatt_client_subscriptions_consume_notification(BLECharacteristic *characteristic_ref_out,
                                                        uint8_t *value_out,
                                                        uint16_t *value_length_in_out,
                                                        GAPLEClient client, bool *has_more_out) {
  return 0;
}

void gatt_client_subscriptions_cleanup_by_client(GAPLEClient client) {

}

void gatt_client_subscriptions_cleanup_by_connection(struct GAPLEConnection *connection,
                                                     bool should_unsubscribe) {

}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Fake Manipulation

void fake_gatt_client_subscriptions_init(void) {
  s_subscribe_return_value = BTErrnoOK;
}

void fake_gatt_client_subscriptions_deinit(void) {
  Subscribe *subscribe = s_subscribe_head;
  while (subscribe) {
    Subscribe *next = (Subscribe *) subscribe->node.next;
    free(subscribe);
    subscribe = next;
  }
  s_subscribe_head = NULL;
}

void fake_gatt_client_subscriptions_set_subscribe_return_value(BTErrno e) {
  s_subscribe_return_value = e;
}

void fake_gatt_client_subscriptions_assert_subscribe(BLECharacteristic characteristic,
                                                     BLESubscription subscription_type,
                                                     GAPLEClient client) {
  if (s_subscribe_head) {
    cl_assert_equal_i(characteristic, s_subscribe_head->characteristic);
    cl_assert_equal_i(subscription_type, s_subscribe_head->subscription_type);
    cl_assert_equal_i(client, s_subscribe_head->client);
  } else {
    cl_assert_(false, "No gatt_client_subscriptions_subscribe() has happened at all");
  }
  s_subscribe_head = (Subscribe *) list_pop_head(&s_subscribe_head->node);
}
