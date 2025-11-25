/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "ble_demo_scan.h"

static Window *s_scan_window;

static void descriptor_write_handler(BLEDescriptor descriptor,
                                     BLEGATTError error) {
  char uuid_buffer[UUID_STRING_BUFFER_LENGTH];
  Uuid descriptor_uuid = ble_descriptor_get_uuid(descriptor);
  uuid_to_string(&descriptor_uuid, uuid_buffer);

  APP_LOG(APP_LOG_LEVEL_INFO, "Write response for Descriptor %s (error=%u)", uuid_buffer, error);
}

static void descriptor_read_handler(BLEDescriptor descriptor,
                                    const uint8_t *value,
                                    size_t value_length,
                                    uint16_t value_offset,
                                    BLEGATTError error) {
  char uuid_buffer[UUID_STRING_BUFFER_LENGTH];
  Uuid descriptor_uuid = ble_descriptor_get_uuid(descriptor);
  uuid_to_string(&descriptor_uuid, uuid_buffer);

  APP_LOG(APP_LOG_LEVEL_INFO, "Read Descriptor %s, %u bytes, error: %u",
          uuid_buffer, value_length, error);
  for (size_t i = 0; i < value_length; ++i) {
    APP_LOG(APP_LOG_LEVEL_INFO, "0x%02x", value[i]);
  }
}

static void read_handler(BLECharacteristic characteristic,
                         const uint8_t *value,
                         size_t value_length,
                         uint16_t value_offset,
                         BLEGATTError error) {
  char uuid_buffer[UUID_STRING_BUFFER_LENGTH];
  Uuid characteristic_uuid = ble_characteristic_get_uuid(characteristic);
  uuid_to_string(&characteristic_uuid, uuid_buffer);

  APP_LOG(APP_LOG_LEVEL_INFO, "Read Characteristic %s, %u bytes, error: %u",
          uuid_buffer, value_length, error);
  for (size_t i = 0; i < value_length; ++i) {
    APP_LOG(APP_LOG_LEVEL_INFO, "0x%02x", value[i]);
  }
}

static void write_handler(BLECharacteristic characteristic,
                          BLEGATTError error) {
  char uuid_buffer[UUID_STRING_BUFFER_LENGTH];
  Uuid characteristic_uuid = ble_characteristic_get_uuid(characteristic);
  uuid_to_string(&characteristic_uuid, uuid_buffer);

  APP_LOG(APP_LOG_LEVEL_INFO, "Write response for Characteristic %s (error=%u)",
          uuid_buffer, error);
}

static void subscribe_handler(BLECharacteristic characteristic,
                              BLESubscription subscription_type,
                              BLEGATTError error) {
  char uuid_buffer[UUID_STRING_BUFFER_LENGTH];
  Uuid characteristic_uuid = ble_characteristic_get_uuid(characteristic);
  uuid_to_string(&characteristic_uuid, uuid_buffer);

  APP_LOG(APP_LOG_LEVEL_INFO, "Subscription to Characteristic %s (subscription_type=%u, error=%u)",
          uuid_buffer, subscription_type, error);
}

static void service_change_handler(BTDevice device,
                                   const BLEService services[],
                                   uint8_t num_services,
                                   BTErrno status) {
  const BTDeviceAddress address = bt_device_get_address(device);

  char uuid_buffer[UUID_STRING_BUFFER_LENGTH];

  for (unsigned int i = 0; i < num_services; ++i) {
    Uuid service_uuid = ble_service_get_uuid(services[i]);
    uuid_to_string(&service_uuid, uuid_buffer);

    APP_LOG(APP_LOG_LEVEL_INFO,
            "Discovered service %s (0x%08x) on " BT_DEVICE_ADDRESS_FMT,
            uuid_buffer,
            services[i],
            BT_DEVICE_ADDRESS_XPLODE(address));

    BLECharacteristic characteristics[8];
    uint8_t num_characteristics =
               ble_service_get_characteristics(services[i], characteristics, 8);
    if (num_characteristics > 8) {
      num_characteristics = 8;
    }
    for (unsigned int c = 0; c < num_characteristics; ++c) {
      Uuid characteristic_uuid = ble_characteristic_get_uuid(characteristics[c]);
      uuid_to_string(&characteristic_uuid, uuid_buffer);

      APP_LOG(APP_LOG_LEVEL_INFO, "-- Characteristic: %s (0x%08x)",
              uuid_buffer, characteristics[c]);

      Uuid device_name_characteristic = bt_uuid_expand_16bit(0x2A00);
      if (uuid_equal(&device_name_characteristic, &characteristic_uuid)) {
        BTErrno err =  ble_client_read(characteristics[c]);
        APP_LOG(APP_LOG_LEVEL_INFO, "Reading... %u", err);
      }

      // If the characteristic is the Alert Control Point, try to write something to it
      const Uuid alert_control_point = bt_uuid_expand_16bit(0x2A44);
      if (uuid_equal(&alert_control_point, &characteristic_uuid)) {
        const char value[] = "Hello World.";
        ble_client_write(characteristics[c], (const uint8_t *) value, strlen(value) + 1);
      }

      const Uuid hrm_uuid = bt_uuid_expand_16bit(0x2A37);
      if (uuid_equal(&hrm_uuid, &characteristic_uuid)) {
        ble_client_subscribe(characteristics[c], BLESubscriptionNotifications);
      }

//      BLEDescriptor descriptors[8];
//      uint8_t num_descriptors =
//              ble_characteristic_get_descriptors(characteristics[c], descriptors, 8);
//      for (unsigned int d = 0; d < num_descriptors; ++d) {
//        const Uuid descriptor_uuid =  ble_descriptor_get_uuid(descriptors[d]);
//        uuid_to_string(&descriptor_uuid, uuid_buffer);
//        APP_LOG(APP_LOG_LEVEL_INFO, "---- Descriptor: %s (0x%08x)", uuid_buffer, descriptors[d]);
//
//        // If the characteristic is the Heart Rate Measurement,
//        // attempt to subscribe to it by writing to the CCCD:
//        const Uuid hrm_uuid = bt_uuid_expand_16bit(0x2A37);
//        const Uuid cccd_uuid = bt_uuid_expand_16bit(0x2902);
//        if (uuid_equal(&hrm_uuid, &characteristic_uuid) &&
//            uuid_equal(&cccd_uuid, &descriptor_uuid)) {
//          APP_LOG(APP_LOG_LEVEL_INFO, "---- Subscribing to Heart Rate Measurement notifications");
//          const uint16_t enable_notifications = 1;
//          ble_client_write_descriptor(descriptors[d],
//                                      (const uint8_t *) &enable_notifications,
//                                      sizeof(enable_notifications));
//        }
//
//        ble_client_read_descriptor(descriptors[d]);
//      }
    }
  }
}

static void connection_handler(BTDevice device, BTErrno connection_status) {
  const BTDeviceAddress address = bt_device_get_address(device);

  const bool connected = (connection_status == BTErrnoConnected);

  APP_LOG(APP_LOG_LEVEL_INFO, "%s " BT_DEVICE_ADDRESS_FMT " (status=%d)",
          connected ? "Connected" : "Disconnected",
          BT_DEVICE_ADDRESS_XPLODE(address), connection_status);

  ble_client_discover_services_and_characteristics(device);
}

int main(void) {
  ble_client_set_descriptor_write_handler(descriptor_write_handler);
  ble_client_set_descriptor_read_handler(descriptor_read_handler);
  ble_client_set_read_handler(read_handler);
  ble_client_set_write_response_handler(write_handler);
  ble_client_set_subscribe_handler(subscribe_handler);
  ble_central_set_connection_handler(connection_handler);
  ble_client_set_service_change_handler(service_change_handler);

  s_scan_window = ble_demo_scan_window_create();

  window_stack_push(s_scan_window, true /* Animated */);

  app_event_loop();

  window_destroy(s_scan_window);
}
