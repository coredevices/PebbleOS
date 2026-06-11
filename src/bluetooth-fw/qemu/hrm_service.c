/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <bluetooth/hrm_service.h>
#include <bluetooth/gap_le_connect.h>
#include <bluetooth/qemu_hrm_bridge.h>
#include <btutil/bt_device.h>
#include <drivers/qemu/qemu_serial.h>
#include <pbl/services/hrm/hrm_manager.h>
#include <system/logging.h>
#include <util/net.h>

#define QEMU_HRM_BRIDGE_VERSION (1)
#define QEMU_HRM_BRIDGE_CONN_HANDLE (0x0001)

static const BTDeviceInternal s_bridge_device = {
  .address = {
    .octets = { 0x51, 0x20, 0x30, 0x40, 0x50, 0x60 },
  },
  .is_random_address = true,
};

static bool s_hrm_service_enabled;
static bool s_hrm_advertising_enabled;
static bool s_bridge_connected;
static bool s_bridge_subscribed;

static bool prv_is_host_service_enabled(void) {
  return s_hrm_service_enabled && s_hrm_advertising_enabled;
}

static void prv_send_service_state(void) {
  const QemuProtocolBLEHrmBridgeServiceState state = {
    .header = {
      .message = QemuBLEHrmBridgeMessage_ServiceState,
      .version = QEMU_HRM_BRIDGE_VERSION,
    },
    .enabled = prv_is_host_service_enabled(),
  };
  qemu_serial_send(QemuProtocol_BLEHrmBridge, (const uint8_t *)&state, sizeof(state));
}

static void prv_send_measurement(const BleHrmServiceMeasurement *measurement) {
  const QemuProtocolBLEHrmBridgeMeasurement packet = {
    .header = {
      .message = QemuBLEHrmBridgeMessage_Measurement,
      .version = QEMU_HRM_BRIDGE_VERSION,
    },
    .bpm = htons(measurement->bpm),
    .is_on_wrist = measurement->is_on_wrist,
  };
  qemu_serial_send(QemuProtocol_BLEHrmBridge, (const uint8_t *)&packet, sizeof(packet));
}

static void prv_connect_bridge_if_needed(void) {
  if (s_bridge_connected) {
    return;
  }

  const BleConnectionCompleteEvent event = {
    .conn_params = {
      .conn_interval_1_25ms = 24,
      .slave_latency_events = 0,
      .supervision_timeout_10ms = 400,
    },
    .peer_address = s_bridge_device,
    .status = HciStatusCode_Success,
    .is_master = false,
    .is_resolved = false,
    .handle = QEMU_HRM_BRIDGE_CONN_HANDLE,
    .mtu = 23,
  };
  bt_driver_handle_le_connection_complete_event(&event);
  s_bridge_connected = true;
}

static void prv_disconnect_bridge_if_needed(void) {
  if (!s_bridge_connected) {
    return;
  }

  if (s_bridge_subscribed) {
    bt_driver_cb_hrm_service_update_subscription(&s_bridge_device, false);
    s_bridge_subscribed = false;
  }

  const BleDisconnectionCompleteEvent event = {
    .peer_address = s_bridge_device,
    .status = HciStatusCode_Success,
    .reason = HciStatusCode_UnknownConnectionIdentifier,
    .handle = QEMU_HRM_BRIDGE_CONN_HANDLE,
  };
  bt_driver_handle_le_disconnection_complete_event(&event);
  s_bridge_connected = false;
}

static void prv_handle_subscription(const QemuProtocolBLEHrmBridgeSubscription *packet) {
  const bool is_subscribed = packet->is_subscribed != 0;
  if (is_subscribed) {
    prv_connect_bridge_if_needed();
    if (!s_bridge_subscribed) {
      s_bridge_subscribed = true;
      bt_driver_cb_hrm_service_update_subscription(&s_bridge_device, true);
    }
  } else {
    prv_disconnect_bridge_if_needed();
  }
}

static void prv_handle_host_sample(const QemuProtocolBLEHrmBridgeHostSample *packet) {
  if (!s_hrm_service_enabled) {
    return;
  }

  const uint16_t bpm = ntohs(packet->bpm);
  if (bpm == 0 || bpm > UINT8_MAX) {
    return;
  }

  const HRMData data = {
    .features = HRMFeature_BPM,
    .hrm_bpm = (uint8_t)bpm,
    .hrm_quality = packet->is_on_wrist ? HRMQuality_Good : HRMQuality_OffWrist,
  };
  hrm_manager_new_data_cb(&data);
}

bool bt_driver_is_hrm_service_supported(void) {
  return true;
}

void bt_driver_hrm_service_enable(bool enable) {
  if (s_hrm_service_enabled == enable) {
    return;
  }

  s_hrm_service_enabled = enable;
  prv_send_service_state();

  if (!enable) {
    prv_disconnect_bridge_if_needed();
  }
}

void bt_driver_qemu_hrm_bridge_set_advertising_enabled(bool enabled) {
  if (s_hrm_advertising_enabled == enabled) {
    return;
  }

  s_hrm_advertising_enabled = enabled;
  prv_send_service_state();

  if (!prv_is_host_service_enabled()) {
    prv_disconnect_bridge_if_needed();
  }
}

void bt_driver_hrm_service_init(void) {
}

void bt_driver_hrm_service_handle_measurement(const BleHrmServiceMeasurement *measurement,
                                              const BTDeviceInternal *permitted_devices,
                                              size_t num_permitted_devices) {
  if (!s_hrm_service_enabled || !s_bridge_subscribed || !measurement) {
    return;
  }

  for (size_t i = 0; i < num_permitted_devices; ++i) {
    if (bt_device_internal_equal(&s_bridge_device, &permitted_devices[i])) {
      prv_send_measurement(measurement);
      return;
    }
  }
}

void bt_driver_hrm_service_handle_subscription(uint16_t conn_handle, uint16_t attr_handle,
                                               bool is_subscribed) {
}

void bt_driver_qemu_hrm_bridge_handle_packet(const uint8_t *data, uint32_t length) {
  if (length < sizeof(QemuProtocolBLEHrmBridgeHeader)) {
    return;
  }

  const QemuProtocolBLEHrmBridgeHeader *header = (const QemuProtocolBLEHrmBridgeHeader *)data;
  if (header->version != QEMU_HRM_BRIDGE_VERSION) {
    PBL_LOG_WRN("Unsupported QEMU HRM bridge version: %u", header->version);
    return;
  }

  switch (header->message) {
    case QemuBLEHrmBridgeMessage_HostReady:
      prv_send_service_state();
      break;
    case QemuBLEHrmBridgeMessage_Subscription:
      if (length == sizeof(QemuProtocolBLEHrmBridgeSubscription)) {
        prv_handle_subscription((const QemuProtocolBLEHrmBridgeSubscription *)data);
      }
      break;
    case QemuBLEHrmBridgeMessage_HostSample:
      if (length == sizeof(QemuProtocolBLEHrmBridgeHostSample)) {
        prv_handle_host_sample((const QemuProtocolBLEHrmBridgeHostSample *)data);
      }
      break;
    default:
      break;
  }
}

bool bt_driver_qemu_hrm_bridge_disconnect(const BTDeviceInternal *peer_address) {
  if (!peer_address || !bt_device_internal_equal(&s_bridge_device, peer_address)) {
    return false;
  }

  prv_disconnect_bridge_if_needed();
  return true;
}
