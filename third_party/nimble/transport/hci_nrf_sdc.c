/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "hci_alloc.h"

#include <errno.h>
#include <string.h>

#include <pbl/drivers/rng.h>
#include <pbl/kernel/assert.h>
#include <pbl/kernel/fatal.h>
#include <pbl/kernel/sem.h>
#include <pbl/kernel/thread.h>
#include <pbl/kernel/time.h>
#include <pbl/logging/logging.h>
#include <pbl/util/byteorder.h>

// NOTE: transport.h needs os_mbuf.h to be included first
// clang-format off
#include <os/os_mbuf.h>
// clang-format on
#include <mpsl_pebble.h>
#include <nimble/ble.h>
#include <nimble/hci_common.h>
#include <nimble/nimble_port_pebble.h>
#include <nimble/transport.h>
#include <nimble/transport_impl.h>

#undef UNUSED
#include <sdc.h>
#include <sdc_hci.h>
#include <sdc_hci_cmd_controller_baseband.h>
#include <sdc_hci_cmd_info_params.h>
#include <sdc_hci_cmd_le.h>
#include <sdc_hci_cmd_link_control.h>
#include <sdc_hci_cmd_status_params.h>
#include <sdc_soc.h>

PBL_LOG_MODULE_DECLARE(nimble, CONFIG_NIMBLE_LOG_LEVEL);

#define PACKET_SIZE 251U

#define CC_HDR_LEN (sizeof(struct ble_hci_ev) + sizeof(struct ble_hci_ev_command_complete))
#define CS_LEN     (sizeof(struct ble_hci_ev) + sizeof(struct ble_hci_ev_command_status))

static_assert(MYNEWT_VAL(BLE_TRANSPORT_EVT_SIZE) >=
              CC_HDR_LEN + sizeof(sdc_hci_ip_supported_commands_t));

static struct pbl_thread s_thread;
PBL_THREAD_STACK_DEFINE(s_stack, CONFIG_NIMBLE_HCI_THREAD_STACK_SIZE);

static PBL_SEM_DEFINE(s_wake, 0, 1);

static uint8_t s_mem[CONFIG_NIMBLE_HCI_NRF_SDC_MEM_SIZE] PBL_ALIGNED(8);
static uint8_t s_rx[HCI_MSG_BUFFER_MAX_SIZE] PBL_ALIGNED(4);
static uint8_t s_tx[HCI_DATA_PACKET_MAX_SIZE] PBL_ALIGNED(4);

// The commands the dispatcher below takes
static const sdc_hci_cmd_ip_read_local_supported_commands_return_t s_supported = {
  .params = {
    .hci_disconnect = 1,
    .hci_read_remote_version_information = 1,
    .hci_set_event_mask = 1,
    .hci_reset = 1,
    .hci_read_transmit_power_level = 1,
    .hci_set_controller_to_host_flow_control = 1,
    .hci_host_buffer_size = 1,
    .hci_host_number_of_completed_packets = 1,
    .hci_read_local_version_information = 1,
    .hci_read_local_supported_features = 1,
    .hci_read_bd_addr = 1,
    .hci_read_rssi = 1,
    .hci_set_event_mask_page_2 = 1,
    .hci_read_authenticated_payload_timeout = 1,
    .hci_write_authenticated_payload_timeout = 1,
    .hci_le_set_event_mask = 1,
    .hci_le_read_buffer_size_v1 = 1,
    .hci_le_read_local_supported_features = 1,
    .hci_le_set_random_address = 1,
    .hci_le_set_advertising_parameters = 1,
    .hci_le_read_advertising_physical_channel_tx_power = 1,
    .hci_le_set_advertising_data = 1,
    .hci_le_set_scan_response_data = 1,
    .hci_le_set_advertising_enable = 1,
    .hci_le_read_filter_accept_list_size = 1,
    .hci_le_clear_filter_accept_list = 1,
    .hci_le_add_device_to_filter_accept_list = 1,
    .hci_le_remove_device_from_filter_accept_list = 1,
    .hci_le_read_channel_map = 1,
    .hci_le_read_remote_features = 1,
    .hci_le_encrypt = 1,
    .hci_le_rand = 1,
    .hci_le_long_term_key_request_reply = 1,
    .hci_le_long_term_key_request_negative_reply = 1,
    .hci_le_set_data_length = 1,
    .hci_le_read_suggested_default_data_length = 1,
    .hci_le_write_suggested_default_data_length = 1,
    .hci_le_add_device_to_resolving_list = 1,
    .hci_le_remove_device_from_resolving_list = 1,
    .hci_le_clear_resolving_list = 1,
    .hci_le_read_resolving_list_size = 1,
    .hci_le_set_address_resolution_enable = 1,
    .hci_le_set_resolvable_private_address_timeout = 1,
    .hci_le_read_maximum_data_length = 1,
    .hci_le_read_phy = 1,
    .hci_le_set_default_phy = 1,
    .hci_le_set_phy = 1,
    .hci_le_read_transmit_power = 1,
    .hci_le_set_privacy_mode = 1,
#ifdef CONFIG_NIMBLE_HCI_NRF_SDC_MULTIROLE
    .hci_le_set_scan_parameters = 1,
    .hci_le_set_scan_enable = 1,
    .hci_le_create_connection = 1,
    .hci_le_create_connection_cancel = 1,
    .hci_le_connection_update = 1,
    .hci_le_set_host_channel_classification = 1,
    .hci_le_enable_encryption = 1,
#endif
  },
};

#define OP(name) SDC_HCI_OPCODE_CMD_##name

// Command Complete, with no return parameters, or with the function's
#define CC(name, fn)                   \
  case OP(name):                       \
    status = sdc_hci_cmd_##fn(params); \
    break;
#define CC_V(name, fn)           \
  case OP(name):                 \
    status = sdc_hci_cmd_##fn(); \
    break;
#define CC_R(name, fn)                             \
  case OP(name):                                   \
    status = sdc_hci_cmd_##fn((void *)rp);         \
    *rp_len = sizeof(sdc_hci_cmd_##fn##_return_t); \
    break;
#define CC_PR(name, fn)                            \
  case OP(name):                                   \
    status = sdc_hci_cmd_##fn(params, (void *)rp); \
    *rp_len = sizeof(sdc_hci_cmd_##fn##_return_t); \
    break;
// Command Status
#define CS(name, fn)                   \
  case OP(name):                       \
    status = sdc_hci_cmd_##fn(params); \
    *cs = true;                        \
    break;

static uint8_t prv_dispatch(uint16_t opcode, const void *params, uint8_t *rp, size_t *rp_len,
                            bool *cs) {
  uint8_t status;

  switch (opcode) {
    CS(LC_DISCONNECT, lc_disconnect)
    CS(LC_READ_REMOTE_VERSION_INFORMATION, lc_read_remote_version_information)

    CC(CB_SET_EVENT_MASK, cb_set_event_mask)
    CC_V(CB_RESET, cb_reset)
    CC_PR(CB_READ_TRANSMIT_POWER_LEVEL, cb_read_transmit_power_level)
    CC(CB_SET_CONTROLLER_TO_HOST_FLOW_CONTROL, cb_set_controller_to_host_flow_control)
    CC(CB_HOST_BUFFER_SIZE, cb_host_buffer_size)
    CC(CB_HOST_NUMBER_OF_COMPLETED_PACKETS, cb_host_number_of_completed_packets)
    CC(CB_SET_EVENT_MASK_PAGE_2, cb_set_event_mask_page_2)
    CC_PR(CB_READ_AUTHENTICATED_PAYLOAD_TIMEOUT, cb_read_authenticated_payload_timeout)
    CC_PR(CB_WRITE_AUTHENTICATED_PAYLOAD_TIMEOUT, cb_write_authenticated_payload_timeout)

    CC_R(IP_READ_LOCAL_VERSION_INFORMATION, ip_read_local_version_information)
    CC_R(IP_READ_LOCAL_SUPPORTED_FEATURES, ip_read_local_supported_features)
    CC_R(IP_READ_BD_ADDR, ip_read_bd_addr)
    case OP(IP_READ_LOCAL_SUPPORTED_COMMANDS):
      memcpy(rp, &s_supported, sizeof(s_supported));
      *rp_len = sizeof(s_supported);
      status = BLE_ERR_SUCCESS;
      break;

      CC_PR(SP_READ_RSSI, sp_read_rssi)

      CC(LE_SET_EVENT_MASK, le_set_event_mask)
      CC_R(LE_READ_BUFFER_SIZE, le_read_buffer_size)
      CC_R(LE_READ_LOCAL_SUPPORTED_FEATURES, le_read_local_supported_features)
      CC(LE_SET_RANDOM_ADDRESS, le_set_random_address)
      CC(LE_SET_ADV_PARAMS, le_set_adv_params)
      CC_R(LE_READ_ADV_PHYSICAL_CHANNEL_TX_POWER, le_read_adv_physical_channel_tx_power)
      CC(LE_SET_ADV_DATA, le_set_adv_data)
      CC(LE_SET_SCAN_RESPONSE_DATA, le_set_scan_response_data)
      CC(LE_SET_ADV_ENABLE, le_set_adv_enable)
      CC_R(LE_READ_FILTER_ACCEPT_LIST_SIZE, le_read_filter_accept_list_size)
      CC_V(LE_CLEAR_FILTER_ACCEPT_LIST, le_clear_filter_accept_list)
      CC(LE_ADD_DEVICE_TO_FILTER_ACCEPT_LIST, le_add_device_to_filter_accept_list)
      CC(LE_REMOVE_DEVICE_FROM_FILTER_ACCEPT_LIST, le_remove_device_from_filter_accept_list)
      CC_PR(LE_READ_CHANNEL_MAP, le_read_channel_map)
      CS(LE_READ_REMOTE_FEATURES, le_read_remote_features)
      CC_PR(LE_ENCRYPT, le_encrypt)
      CC_R(LE_RAND, le_rand)
      CC_PR(LE_LONG_TERM_KEY_REQUEST_REPLY, le_long_term_key_request_reply)
      CC_PR(LE_LONG_TERM_KEY_REQUEST_NEGATIVE_REPLY, le_long_term_key_request_negative_reply)
      CC_PR(LE_SET_DATA_LENGTH, le_set_data_length)
      CC_R(LE_READ_SUGGESTED_DEFAULT_DATA_LENGTH, le_read_suggested_default_data_length)
      CC(LE_WRITE_SUGGESTED_DEFAULT_DATA_LENGTH, le_write_suggested_default_data_length)
      CC(LE_ADD_DEVICE_TO_RESOLVING_LIST, le_add_device_to_resolving_list)
      CC(LE_REMOVE_DEVICE_FROM_RESOLVING_LIST, le_remove_device_from_resolving_list)
      CC_V(LE_CLEAR_RESOLVING_LIST, le_clear_resolving_list)
      CC_R(LE_READ_RESOLVING_LIST_SIZE, le_read_resolving_list_size)
      CC(LE_SET_ADDRESS_RESOLUTION_ENABLE, le_set_address_resolution_enable)
      CC(LE_SET_RESOLVABLE_PRIVATE_ADDRESS_TIMEOUT, le_set_resolvable_private_address_timeout)
      CC_R(LE_READ_MAX_DATA_LENGTH, le_read_max_data_length)
      CC_PR(LE_READ_PHY, le_read_phy)
      CC(LE_SET_DEFAULT_PHY, le_set_default_phy)
      CS(LE_SET_PHY, le_set_phy)
      CC_R(LE_READ_TRANSMIT_POWER, le_read_transmit_power)
      CC(LE_SET_PRIVACY_MODE, le_set_privacy_mode)

#ifdef CONFIG_NIMBLE_HCI_NRF_SDC_MULTIROLE
      CC(LE_SET_SCAN_PARAMS, le_set_scan_params)
      CC(LE_SET_SCAN_ENABLE, le_set_scan_enable)
      CS(LE_CREATE_CONN, le_create_conn)
      CC_V(LE_CREATE_CONN_CANCEL, le_create_conn_cancel)
      CS(LE_CONN_UPDATE, le_conn_update)
      CC(LE_SET_HOST_CHANNEL_CLASSIFICATION, le_set_host_channel_classification)
      CS(LE_ENABLE_ENCRYPTION, le_enable_encryption)
#endif

    default:
      status = BLE_ERR_UNKNOWN_HCI_CMD;
      break;
  }

  return status;
}

static void prv_to_hs_evt(const uint8_t *ev) {
  const bool discardable =
      (ev[0] == BLE_HCI_EVCODE_LE_META) &&
      (ev[2] == BLE_HCI_LE_SUBEV_ADV_RPT || ev[2] == BLE_HCI_LE_SUBEV_EXT_ADV_RPT);
  const size_t len = sizeof(struct ble_hci_ev) + ev[1];
  uint8_t *buf;

  if (len > MYNEWT_VAL(BLE_TRANSPORT_EVT_SIZE)) {
    PBL_LOG_ERR("Event 0x%02x too long (%u)", ev[0], (unsigned int)len);
    return;
  }

  buf = nimble_hci__allocs_from_ll.evt(discardable);
  if (buf == NULL) {
    return;
  }

  memcpy(buf, ev, len);
  (void)ble_transport_to_hs_evt(buf);
}

static void prv_to_hs_acl(const uint8_t *pkt) {
  const size_t len = 4U + pbl_get_le16(&pkt[2]);
  struct os_mbuf *om = nimble_hci__allocs_from_ll.acl();

  if (os_mbuf_append(om, pkt, len) != 0) {
    os_mbuf_free_chain(om);
    PBL_LOG_ERR("ACL of %u bytes dropped", (unsigned int)len);
    return;
  }

  (void)ble_transport_to_hs_acl(om);
}

static void prv_thread_main(void *arg) {
  for (;;) {
    uint8_t type;
    int32_t ret;

    (void)pbl_sem_take(&s_wake, PBL_FOREVER);

    do {
      mpsl_pebble_lock();
      ret = sdc_hci_get(s_rx, &type);
      mpsl_pebble_unlock();

      if (ret != 0) {
        break;
      }

      if (type == SDC_HCI_MSG_TYPE_EVT) {
        prv_to_hs_evt(s_rx);
      } else if (type == SDC_HCI_MSG_TYPE_DATA) {
        prv_to_hs_acl(s_rx);
      }
    } while (true);
  }
}

static void prv_sdc_callback(void) {
  pbl_sem_give(&s_wake);
}

static void prv_fault(const char *file, const uint32_t line) {
  pbl_panic_at(file, (int)line, "Controller fault");
}

static void prv_rand(uint8_t *buf, uint8_t len) {
  while (len > 0U) {
    uint32_t word;
    const uint8_t n = (len < sizeof(word)) ? len : sizeof(word);

    PBL_ASSERTN(rng_rand(&word) == 0);
    memcpy(buf, &word, n);
    buf += n;
    len -= n;
  }
}

static int32_t prv_cfg(uint8_t type, const sdc_cfg_t *cfg) {
  return sdc_cfg_set(SDC_DEFAULT_RESOURCE_CFG_TAG, type, cfg);
}

static int prv_configure(void) {
  sdc_cfg_t cfg;
  int32_t mem = 0;

  sdc_support_adv();
  sdc_support_peripheral();
  sdc_support_dle_peripheral();
  sdc_support_le_2m_phy();
  sdc_support_phy_update_peripheral();
#ifdef CONFIG_NIMBLE_HCI_NRF_SDC_MULTIROLE
  sdc_support_scan();
  sdc_support_central();
  sdc_support_dle_central();
  sdc_support_phy_update_central();
#endif

  cfg = (sdc_cfg_t){.peripheral_count = {.count = 1U}};
  mem = prv_cfg(SDC_CFG_TYPE_PERIPHERAL_COUNT, &cfg);
#ifdef CONFIG_NIMBLE_HCI_NRF_SDC_MULTIROLE
  if (mem >= 0) {
    cfg = (sdc_cfg_t){.central_count = {.count = 1U}};
    mem = prv_cfg(SDC_CFG_TYPE_CENTRAL_COUNT, &cfg);
  }
#endif
  if (mem >= 0) {
    cfg = (sdc_cfg_t){
      .buffer_cfg = {
        .tx_packet_size = PACKET_SIZE,
        .rx_packet_size = PACKET_SIZE,
        .tx_packet_count = CONFIG_NIMBLE_HCI_NRF_SDC_TX_PACKETS,
        .rx_packet_count = CONFIG_NIMBLE_HCI_NRF_SDC_RX_PACKETS,
      }
    };
    mem = prv_cfg(SDC_CFG_TYPE_BUFFER_CFG, &cfg);
  }
  if (mem >= 0) {
    cfg = (sdc_cfg_t){.adv_count = {.count = 1U}};
    mem = prv_cfg(SDC_CFG_TYPE_ADV_COUNT, &cfg);
  }
  if (mem < 0) {
    PBL_LOG_ERR("Controller configuration failed (%d)", (int)mem);
    return -EINVAL;
  }

  if ((size_t)mem > sizeof(s_mem)) {
    PBL_LOG_ERR("Controller needs %d bytes, has %u", (int)mem, (unsigned int)sizeof(s_mem));
    return -ENOMEM;
  }

  PBL_LOG_DBG("Controller memory: %d of %u bytes", (int)mem, (unsigned int)sizeof(s_mem));

  return 0;
}

void ble_transport_ll_init(void) {
  static const sdc_rand_source_t rand = {.rand_poll = prv_rand};
  int ret;

  mpsl_pebble_lock();

  ret = sdc_init(prv_fault);
  PBL_ASSERT(ret == 0, "Controller init failed (%d)", ret);

  ret = prv_configure();
  PBL_ASSERTN(ret == 0);

  ret = sdc_rand_source_register(&rand);
  PBL_ASSERT(ret == 0, "Controller entropy failed (%d)", ret);

  ret = sdc_enable(prv_sdc_callback, s_mem);
  PBL_ASSERT(ret == 0, "Controller enable failed (%d)", ret);

  mpsl_pebble_unlock();

  nimble_hci__alloc_init();

  struct pbl_thread_attr attr = {
    .name = "NimbleHCI",
    .entry = prv_thread_main,
    .prio = CONFIG_NIMBLE_HCI_THREAD_PRIO,
    .privileged = true,
    .stack = s_stack,
    .stack_size = sizeof(s_stack),
  };

  PBL_ASSERTN(pbl_thread_create(&s_thread, &attr) == 0);
}

int ble_transport_to_ll_cmd_impl(void *buf) {
  const struct ble_hci_cmd *cmd = buf;
  const uint16_t opcode = pbl_get_le16(&cmd->opcode);
  uint8_t *ev = ble_transport_alloc_evt(0);
  size_t rp_len = 0;
  bool cs = false;
  uint8_t status;

  if (ev == NULL) {
    ble_transport_free(buf);
    PBL_LOG_ERR("No event buffer for command 0x%04x", opcode);
    return BLE_ERR_MEM_CAPACITY;
  }

  mpsl_pebble_lock();
  status = prv_dispatch(opcode, cmd->data, &ev[CC_HDR_LEN], &rp_len, &cs);
  mpsl_pebble_unlock();

  ble_transport_free(buf);

  // The host flow control command is answered only when it fails
  if (opcode == OP(CB_HOST_NUMBER_OF_COMPLETED_PACKETS) && status == BLE_ERR_SUCCESS) {
    ble_transport_free(ev);
    return 0;
  }

  if (cs) {
    struct ble_hci_ev_command_status *st = (void *)&ev[sizeof(struct ble_hci_ev)];

    ev[0] = BLE_HCI_EVCODE_COMMAND_STATUS;
    ev[1] = sizeof(*st);
    st->status = status;
    st->num_packets = 1U;
    pbl_put_le16(opcode, &st->opcode);
  } else {
    struct ble_hci_ev_command_complete *cc = (void *)&ev[sizeof(struct ble_hci_ev)];

    ev[0] = BLE_HCI_EVCODE_COMMAND_COMPLETE;
    ev[1] = (uint8_t)(sizeof(*cc) + rp_len);
    cc->num_packets = 1U;
    pbl_put_le16(opcode, &cc->opcode);
    cc->status = status;
  }

  (void)ble_transport_to_hs_evt(ev);

  pbl_sem_give(&s_wake);

  return 0;
}

int ble_transport_to_ll_acl_impl(struct os_mbuf *om) {
  const uint16_t len = OS_MBUF_PKTLEN(om);
  int32_t ret = -1;

  if (len <= sizeof(s_tx) && os_mbuf_copydata(om, 0, len, s_tx) == 0) {
    mpsl_pebble_lock();
    ret = sdc_hci_data_put(s_tx);
    mpsl_pebble_unlock();
  }

  os_mbuf_free_chain(om);

  if (ret != 0) {
    PBL_LOG_ERR("ACL of %u bytes rejected (%d)", len, (int)ret);
    return BLE_ERR_MEM_CAPACITY;
  }

  pbl_sem_give(&s_wake);

  return 0;
}

int ble_transport_to_ll_iso_impl(struct os_mbuf *om) {
  os_mbuf_free_chain(om);

  return BLE_ERR_UNSUPPORTED;
}

struct pbl_thread *nimble_port_pebble_hci_thread(void) {
  return &s_thread;
}
