/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/bluetooth/types.h>

typedef enum {
  BLEClientServicesAdded,
  BLEClientServicesRemoved,
  BLEClientServicesInvalidateAll
} BLEClientServiceChangeUpdate;

//! Callback that is called when the services on a remote device that are
//! available to the application have changed. It gets called when:
//!  + the initial discovery of services completes
//!  + a new service has been discovered on the device after initial discovery
//!  + a service has been removed
//!  + the remote device has disconnected
//!
//! @note For convenience, the services are owned by the system and references
//! to services, characteristics and descriptors are guaranteed to remain valid
//! *until the service exposing the characteristic or descriptor is removed or
//! all services are invalidated* or until application is terminated.
//! @note References to services, characteristics and descriptors for the
//! specified device, that have been obtained in a previous callback to the
//! handler, are no longer valid. The application is responsible for cleaning up
//! these references.
//!
//! @param device The device associated with the service discovery process
//! @param update_type Defines what type of service update is being received
//! @param services Array of pointers to the discovered services. The array will
//! only contain Service UUIDs that matches the filter that was passed into the
//! ble_client_discover_services_and_characteristics() call.
//! @param num_services The number of discovered services matching the criteria.
//! @param status PBL_BT_ERRNO_OK if the service discovery was completed successfully.
//! If the device was disconnected, PBL_BT_ERRNO_CONNECTION_TIMEOUT,
//! PBL_BT_ERRNO_REMOTELY_TERMINATED, PBL_BT_ERRNO_LOCALLY_TERMINATED_BY_SYSTEM or
//! PBL_BT_ERRNO_LOCALLY_TERMINATED_BY_APP will specify the reason of the disconnection.
//! The number of services will be zero upon a disconnection.
typedef void (*BLEClientServiceChangeHandler)(struct pbl_bt_device device,
                                              BLEClientServiceChangeUpdate update_type,
                                              const pbl_bt_service_t services[],
                                              uint8_t num_services, enum pbl_bt_errno status);

//! Registers the callback that handles service changes. After the call to
//! this function returns, the application should be ready to handle calls to
//! the handler.
//! @param handler Pointer to the function that will handle the service changes
//! @return PBL_BT_ERRNO_OK if the handler was registered successfully,
//! or TODO....
enum pbl_bt_errno ble_client_set_service_change_handler(BLEClientServiceChangeHandler handler);

//! Registers the filter list of Service UUIDs.
//! @param service_uuids An array of the Service UUIDs that the application is
//! interested in and the system should filter by. Passing NULL will discover
//! all services on the device.
//! @param num_uuids The number of Uuids in the service_uuids array. Ignored
//! when NULL is passed for the service_uuids argument.
//! @return PBL_BT_ERRNO_OK if the filter was set up successfully,
//! or TODO....
enum pbl_bt_errno ble_client_set_service_filter(const Uuid service_uuids[], uint8_t num_uuids);

//! Starts a discovery of services and characteristics on a remote device.
//! The discovered services will be delivered to the application through the
//! BLEClientServiceChangeHandler. The results will be filtered with the list
//! of Service UUIDs as configured with ble_client_set_service_filter().
//! @param device The device for which to perform service discovery.
//! @return PBL_BT_ERRNO_OK if the service discovery started successfully,
//! PBL_BT_ERRNO_INVALID_PARAMETER if the device was not connected,
//! PBL_BT_ERRNO_INVALID_STATE if service discovery was already on-going, or
//! an internal error otherwise (>= PBL_BT_ERRNO_INTERNAL_ERROR_BEGIN).
enum pbl_bt_errno ble_client_discover_services_and_characteristics(struct pbl_bt_device device);

//! Different subscription types that can be used with ble_client_subscribe()
typedef enum {
  //! No subscription.
  BLESubscriptionNone = 0,
  //! Notification subscription.
  BLESubscriptionNotifications = (1 << 0),
  //! Indication subscription.
  BLESubscriptionIndications = (1 << 1),
  //! Any subscription. Use this value with ble_client_subscribe(), in case
  //! the application does not care about the type of subscription. If both
  //! types are supported by the server, the notification subscription type
  //! will be used.
  BLESubscriptionAny = BLESubscriptionNotifications | BLESubscriptionIndications,
} BLESubscription;

//! Callback to receive the characteristic value, resulting from either
//! ble_client_read() and/or ble_client_subscribe().
//! @param characteristic The characteristic of the received value
//! @param value Byte-array containing the value
//! @param value_length The number of bytes the byte-array contains
//! @param value_offset The offset in bytes from the start of the characteristic
//! value that has been read.
//! @param error The error or status as returned by the remote server. If the
//! read was successful, this remote server is supposed to send
//! PBL_BT_GATT_ERROR_SUCCESS.
typedef void (*BLEClientReadHandler)(pbl_bt_characteristic_t characteristic, const uint8_t *value,
                                     size_t value_length, uint16_t value_offset,
                                     enum pbl_bt_gatt_error error);

//! Callback to handle the response to a written characteristic, resulting from
//! ble_client_write().
//! @param characteristic The characteristic that was written to.
//! @param error The error or status as returned by the remote server. If the
//! write was successful, this remote server is supposed to send
//! PBL_BT_GATT_ERROR_SUCCESS.
typedef void (*BLEClientWriteHandler)(pbl_bt_characteristic_t characteristic,
                                      enum pbl_bt_gatt_error error);

//! Callback to handle the confirmation of a subscription or unsubscription to
//! characteristic value changes (notifications or indications).
//! @param characteristic The characteristic for which the client is now
//! (un)subscribed.
//! @param subscription_type The type of subscription. If the client is now
//! unsubscribed, the type will be BLESubscriptionNone.
//! @param error The error or status as returned by the remote server. If the
//! (un)subscription was successful, this remote server is supposed to send
//! PBL_BT_GATT_ERROR_SUCCESS.
typedef void (*BLEClientSubscribeHandler)(pbl_bt_characteristic_t characteristic,
                                          BLESubscription subscription_type,
                                          enum pbl_bt_gatt_error error);

//! Callback to handle the event that the buffer for outbound data is empty.
typedef void (*BLEClientBufferEmptyHandler)(void);

//! Registers the handler for characteristic value read operations.
//! @param read_handler Pointer to the function that will handle callbacks
//! with read characteristic values as result of calls to ble_client_read().
//! @return PBL_BT_ERRNO_OK if the handlers were successfully registered, or ... TODO
enum pbl_bt_errno ble_client_set_read_handler(BLEClientReadHandler read_handler);

//! Registers the handler for characteristic value write (with response)
//! operations.
//! @param write_handler Pointer to the function that will handle callbacks
//! for written characteristic values as result of calls to ble_client_write().
//! @return PBL_BT_ERRNO_OK if the handlers were successfully registered, or ... TODO
enum pbl_bt_errno ble_client_set_write_response_handler(BLEClientWriteHandler write_handler);

//! Registers the handler for characteristic value subscribe operations.
//! @param subscribe_handler Pointer to the function that will handle callbacks
//! for (un)subscription confirmations as result of calls to
//! ble_client_subscribe().
//! @return PBL_BT_ERRNO_OK if the handlers were successfully registered, or ... TODO
enum pbl_bt_errno ble_client_set_subscribe_handler(BLEClientSubscribeHandler subscribe_handler);

//! Registers the handler to get called back when the buffer for outbound
//! data is empty again.
//! @param empty_handler Pointer to the function that will handle callbacks
//! for "buffer empty" events.
//! @return PBL_BT_ERRNO_OK if the handlers were successfully registered, or ... TODO
enum pbl_bt_errno ble_client_set_buffer_empty_handler(BLEClientBufferEmptyHandler empty_handler);

//! Gets the maximum characteristic value size that can be written. This size
//! can vary depending on the connected device.
//! @param device The device for which to get the maximum characteristic value
//! size.
//! @return The maximum characteristic value size that can be written.
uint16_t ble_client_get_maximum_value_length(struct pbl_bt_device device);

//! Read the value of a characteristic.
//! A call to this function will result in a callback to the registered
//! BLEClientReadHandler handler. @see ble_client_set_read_handler.
//! @param characteristic The characteristic for which to read the value
//! @return PBL_BT_ERRNO_OK if the operation was successfully started, or ... TODO
enum pbl_bt_errno ble_client_read(pbl_bt_characteristic_t characteristic);

//! Write the value of a characterstic.
//! A call to this function will result in a callback to the registered
//! BLEClientWriteHandler handler. @see ble_client_set_write_response_handler.
//! @param characteristic The characteristic for which to write the value
//! @param value Buffer with the value to write
//! @param value_length Number of bytes to write
//! @note Values must not be longer than ble_client_get_maximum_value_length().
//! @return PBL_BT_ERRNO_OK if the operation was successfully started, or ... TODO
enum pbl_bt_errno ble_client_write(pbl_bt_characteristic_t characteristic, const uint8_t *value,
                                   size_t value_length);

//! Write the value of a characterstic without response.
//! @param characteristic The characteristic for which to write the value
//! @param value Buffer with the value to write
//! @param value_length Number of bytes to write
//! @note Values must not be longer than ble_client_get_maximum_value_length().
//! @return PBL_BT_ERRNO_OK if the operation was successfully started, or ... TODO
//! If the buffer for outbound data was full, PBL_BT_ERRNO_NOT_ENOUGH_RESOURCES will
//! be returned. When the buffer is emptied, the handler that is registered
//! using ble_client_set_buffer_empty_handler() will be called.
enum pbl_bt_errno ble_client_write_without_response(pbl_bt_characteristic_t characteristic,
                                                    const uint8_t *value, size_t value_length);

//! Subscribe to be notified or indicated of value changes of a characteristic.
//!
//! The value updates are delivered to the application through the
//! BLEClientReadHandler, which should be registered before calling this
//! function using ble_client_set_read_handler().
//!
//! There are two types of subscriptions: notifications and indications.
//! For notifications there is no flow-control. This means that notifications
//! can get dropped if the rate at which they are sent is too high. Conversely,
//! each indication needs an acknowledgement from the receiver before the next
//! one can get sent and is thus more reliable. The system performs
//! acknowledgements to indications automatically. Applications do not need to
//! worry about this, nor can they affect this.
//! @param characteristic The characteristic to subscribe to.
//! @param subscription_type The type of subscription to use.
//! If BLESubscriptionAny is used as subscription_type and both types are
//! supported by the server, the notification subscription type will be used.
//! @note This call does not block and returns quickly. A callback to
//! the BLEClientSubscribeHandler will happen at a later point in time, to
//! report the success or failure of the subscription. This handler should be
//! registered before calling this function using
//! ble_client_set_subscribe_handler().
//! @note Under the hood, this API writes to the Client Characteristic
//! Configuration Descriptor's Notifications or Indications enabled/disabled
//! bit.
//! @return PBL_BT_ERRNO_OK if the subscription request was sent successfully, or
//! TODO...
enum pbl_bt_errno ble_client_subscribe(pbl_bt_characteristic_t characteristic,
                                       BLESubscription subscription_type);

//! Callback to receive the descriptor value, resulting from a call to
//! ble_client_read_descriptor().
//! @param descriptor The descriptor of the received value
//! @param value Byte-array containing the value
//! @param value_length The number of bytes the byte-array contains
//! @param value_offset The offset in bytes from the start of the descriptor
//! value that has been read.
//! @param error The error or status as returned by the remote server. If the
//! read was successful, this remote server is supposed to send
//! PBL_BT_GATT_ERROR_SUCCESS.
typedef void (*BLEClientReadDescriptorHandler)(pbl_bt_descriptor_t descriptor, const uint8_t *value,
                                               size_t value_length, uint16_t value_offset,
                                               enum pbl_bt_gatt_error error);

//! Callback to handle the response to a written descriptor, resulting from
//! ble_client_write_descriptor().
//! @param descriptor The descriptor that was written to.
//! @param error The error or status as returned by the remote server. If the
//! write was successful, this remote server is supposed to send
//! PBL_BT_GATT_ERROR_SUCCESS.
typedef void (*BLEClientWriteDescriptorHandler)(pbl_bt_descriptor_t descriptor,
                                                enum pbl_bt_gatt_error error);

//! Registers the handlers for descriptor value write operations.
//! @param write_handler Pointer to the function that will handle callbacks
//! for written descriptor values as result of calls to
//! ble_client_write_descriptor().
//! @return PBL_BT_ERRNO_OK if the handlers were successfully registered, or ... TODO
enum pbl_bt_errno ble_client_set_descriptor_write_handler(
    BLEClientWriteDescriptorHandler write_handler);

//! Registers the handlers for descriptor value read operations.
//! @param read_handler Pointer to the function that will handle callbacks
//! with read descriptor values as result of calls to
//! ble_client_read_descriptor().
//! @return PBL_BT_ERRNO_OK if the handlers were successfully registered, or ... TODO
enum pbl_bt_errno ble_client_set_descriptor_read_handler(
    BLEClientReadDescriptorHandler read_handler);

//! Write the value of a descriptor.
//! A call to this function will result in a callback to the registered
//! BLEClientWriteDescriptorHandler handler.
//! @see ble_client_set_descriptor_write_handler.
//! @param descriptor The descriptor for which to write the value
//! @param value Buffer with the value to write
//! @param value_length Number of bytes to write
//! @note Values must not be longer than ble_client_get_maximum_value_length().
//! @return PBL_BT_ERRNO_OK if the operation was successfully started, or ... TODO
enum pbl_bt_errno ble_client_write_descriptor(pbl_bt_descriptor_t descriptor, const uint8_t *value,
                                              size_t value_length);

//! Read the value of a descriptor.
//! A call to this function will result in a callback to the registered
//! BLEClientReadDescriptorHandler handler.
//! @see ble_client_set_descriptor_read_handler.
//! @param descriptor The descriptor for which to read the value
//! @return PBL_BT_ERRNO_OK if the operation was successfully started, or ... TODO
enum pbl_bt_errno ble_client_read_descriptor(pbl_bt_descriptor_t descriptor);

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// (FUTURE / LATER / NOT SCOPED)
// Just to see how symmetric the Server APIs would be:

//! Opaque ATT request context
typedef void *BLERequest;

typedef void (*BLEServerWriteHandler)(BLERequest request, pbl_bt_characteristic_t characteristic,
                                      struct pbl_bt_device remote_device, const uint8_t *value,
                                      size_t value_length, uint16_t value_offset);

typedef void (*BLEServerReadHandler)(pbl_bt_characteristic_t characteristic,
                                     struct pbl_bt_device remote_device, uint16_t value_offset);

typedef void (*BLEServerSubscribeHandler)(pbl_bt_characteristic_t characteristic,
                                          struct pbl_bt_device remote_device,
                                          BLESubscription subscription_type);

enum pbl_bt_errno ble_server_set_handlers(BLEServerReadHandler read_handler,
                                          BLEServerWriteHandler write_handler,
                                          BLEServerSubscribeHandler subscription_handler);

enum pbl_bt_errno ble_server_start_service(pbl_bt_service_t service);

enum pbl_bt_errno ble_server_stop_service(pbl_bt_service_t service);

enum pbl_bt_errno ble_server_respond_to_write(BLERequest request, enum pbl_bt_gatt_error error);

enum pbl_bt_errno ble_server_respond_to_read(BLERequest request, enum pbl_bt_gatt_error error,
                                             const uint8_t *value, size_t value_length,
                                             uint16_t value_offset);

enum pbl_bt_errno ble_server_send_update(pbl_bt_characteristic_t characteristic,
                                         const uint8_t *value, size_t value_length);

enum pbl_bt_errno ble_server_send_update_selectively(pbl_bt_characteristic_t characteristic,
                                                     const uint8_t *value, size_t value_length,
                                                     const struct pbl_bt_device *devices,
                                                     uint8_t num_devices);
