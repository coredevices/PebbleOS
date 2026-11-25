/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "fake_gatt_client_subscriptions.h"

#include <clar_asserts.h>

typedef struct {
  ListNode node;
  pbl_bt_characteristic_t characteristic;
  BLESubscription subscription_type;
} Subscribe;

static Subscribe *s_subscribe_head;

static int s_subscribe_return_value;

int gatt_client_subscriptions_subscribe(pbl_bt_characteristic_t characteristic,
                                        BLESubscription subscription_type) {
  Subscribe *subscribe = malloc(sizeof(Subscribe));
  *subscribe = (const Subscribe){
    .characteristic = characteristic,
    .subscription_type = subscription_type,
  };
  if (s_subscribe_head) {
    list_append((ListNode *)s_subscribe_head, &subscribe->node);
  } else {
    s_subscribe_head = subscribe;
  }
  return s_subscribe_return_value;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Fake Manipulation

void fake_gatt_client_subscriptions_init(void) {
  s_subscribe_return_value = 0;
}

void fake_gatt_client_subscriptions_deinit(void) {
  Subscribe *subscribe = s_subscribe_head;
  while (subscribe) {
    Subscribe *next = (Subscribe *)subscribe->node.next;
    free(subscribe);
    subscribe = next;
  }
  s_subscribe_head = nullptr;
}

void fake_gatt_client_subscriptions_set_subscribe_return_value(int ret) {
  s_subscribe_return_value = ret;
}

void fake_gatt_client_subscriptions_assert_subscribe(pbl_bt_characteristic_t characteristic,
                                                     BLESubscription subscription_type) {
  if (s_subscribe_head) {
    cl_assert_equal_i(characteristic, s_subscribe_head->characteristic);
    cl_assert_equal_i(subscription_type, s_subscribe_head->subscription_type);
  } else {
    cl_assert_(false, "No gatt_client_subscriptions_subscribe() has happened at all");
  }
  s_subscribe_head = (Subscribe *)list_pop_head(&s_subscribe_head->node);
}
