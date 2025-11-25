/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "fake_HCIAPI.h"

#include "bluetopia_interface.h"

#include "HCIAPI.h"

#include "util/list.h"

#include <stdlib.h>

typedef struct {
  ListNode node;
  Byte_t Address_Type;
  BD_ADDR_t Address;
} WhitelistEntry;

static WhitelistEntry *s_head;

static uint32_t s_whitelist_error_count;

#define MAX_CC2564_WHITELIST_ENTRIES (25)

int HCI_LE_Read_Advertising_Channel_Tx_Power(unsigned int BluetoothStackID,
                                             Byte_t *StatusResult,
                                             Byte_t *Transmit_Power_LevelResult) {
  *Transmit_Power_LevelResult = -55;
  return 0;
}

static bool prv_whitelist_filter(ListNode *found_node, void *data) {
  const WhitelistEntry *entry1 = (WhitelistEntry *) found_node;
  const WhitelistEntry *entry2 = (WhitelistEntry *) data;
  return COMPARE_BD_ADDR(entry1->Address, entry2->Address) &&
         entry1->Address_Type == entry2->Address_Type;
}

static WhitelistEntry * prv_find_whitelist_entry(const WhitelistEntry *model) {
  return (WhitelistEntry *) list_find(&s_head->node,
                                      prv_whitelist_filter,
                                      (void *) model);
}

int HCI_LE_Rand(unsigned int BluetoothStackID, Byte_t *StatusResult,
                Random_Number_t *Random_NumberResult) {
  uint8_t *data = (uint8_t *) Random_NumberResult;
  for (int i = 0; i < sizeof(*Random_NumberResult); ++i) {
    data[i] = i;
  }
  *StatusResult = 0;
  return 0;
}

int HCI_LE_Add_Device_To_White_List(unsigned int BluetoothStackID,
                                    Byte_t Address_Type,
                                    BD_ADDR_t Address,
                                    Byte_t *StatusResult) {
  const uint32_t count = list_count(&s_head->node);
  if (count > MAX_CC2564_WHITELIST_ENTRIES) {
    ++s_whitelist_error_count;
    return -1;
  }

  const WhitelistEntry model = {
    .Address_Type = Address_Type,
    .Address = Address,
  };

  {
    WhitelistEntry *e = prv_find_whitelist_entry(&model);
    // Already present
    if (e) {
      ++s_whitelist_error_count;
      return -1;
    }
  }

  WhitelistEntry *e = (WhitelistEntry *) malloc(sizeof(WhitelistEntry));
  *e = (const WhitelistEntry) {
    .Address_Type = Address_Type,
    .Address = Address,
  };
  s_head = (WhitelistEntry *) list_prepend(&s_head->node, &e->node);
  return 0;
}

int HCI_LE_Remove_Device_From_White_List(unsigned int BluetoothStackID,
                                         Byte_t Address_Type,
                                         BD_ADDR_t Address,
                                         Byte_t *StatusResult) {
  const WhitelistEntry model = {
    .Address_Type = Address_Type,
    .Address = Address,
  };
  WhitelistEntry *e = prv_find_whitelist_entry(&model);
  if (e) {
    list_remove(&e->node, (ListNode **) &s_head, NULL);
    free(e);
    return 0;
  } else {
    // Doesn't exist
    ++s_whitelist_error_count;
    return -1;
  }
}

bool fake_HCIAPI_whitelist_contains(const BTDeviceInternal *device) {
  const WhitelistEntry model = {
    .Address_Type = device->is_random_address ? 0x01 : 0x00,
    .Address = BTDeviceAddressToBDADDR(device->address),
  };
  return (prv_find_whitelist_entry(&model) != NULL);
}

uint32_t fake_HCIAPI_whitelist_count(void) {
  return list_count(&s_head->node);
}

uint32_t fake_HCIAPI_whitelist_error_count(void) {
  return s_whitelist_error_count;
}

void fake_HCIAPI_deinit(void) {
  WhitelistEntry *e = s_head;
  while (e) {
    WhitelistEntry *next = (WhitelistEntry *) e->node.next;
    free(e);
    e = next;
  }
  s_head = NULL;

  s_whitelist_error_count = 0;
}

void cc2564A_advert_no_sleep_wa(void) {
}
