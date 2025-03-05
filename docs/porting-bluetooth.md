# Porting a new Bluetooth stack into PebbleOS

## Build notes

Unlike many other things in the build, a platform textually selects a
Bluetooth controller using the `waf` environment variable,
`conf.env.bt_controller`.  This ends up pulling in a whole sublibrary
(essentially), in `src/bluetooth-fw`.  This is somewhat cleaner than the
rest of the driver stack, but it is a change from what you might expect from
the respect of PebbleOS!

## API surface

A Bluetooth driver exports 70-ish APIs, and has a handful of callbacks that
it needs to trigger at appropriate times.  I group these below in terms of
API families that one should implement, roughly in order of how critical it
is to implement them to get anything at all working.

### Power and identity

* bt_driver_init
* bt_driver_start
* bt_driver_stop
* bt_driver_power_down_controller_on_boot
* bt_driver_id_set_local_device_name 
* bt_driver_id_copy_local_identity_address
* bt_driver_id_copy_chip_info_string
* bt_driver_id_generate_private_resolvable_address
* bt_driver_supports_bt_classic
* bt_driver_set_local_address

* bt_driver_comm_schedule_send_next_job -- you probably just want to have
  these run on KernelMain, unless you have your own thread that sends should
  happen from.  copy from qemu
* bt_driver_comm_is_current_task_send_next_task

### Advertising

* bt_driver_adv_reconnect_get_job_terms
* bt_driver_advert_advertising_disable 
* bt_driver_advert_client_get_tx_power
* bt_driver_advert_set_advertising_data
* bt_driver_advert_advertising_enable


### GAP

* bt_driver_gap_le_disconnect
* bt_driver_gap_le_device_name_request
* bt_driver_gap_le_device_name_request_all
* bt_driver_le_connection_parameter_update
* bt_driver_handle_le_connection_handle_update_address_and_irk
* bt_driver_handle_peer_version_info_event
* bt_driver_handle_le_connection_complete_event
* bt_driver_handle_le_disconnection_complete_event
* bt_driver_handle_le_encryption_change_event
* bt_driver_handle_le_conn_params_update_event


### Pairing and pairing service

Bond database in-memory is managed by the controller.  Bond database in
flash is managed by the OS.

* bt_driver_cb_pairing_confirm_handle_request -- a GAP LE connection wants
  to bond, post message to UI to say so
* bt_driver_pairing_confirm -- UI agrees to do it
* bt_driver_cb_pairing_confirm_handle_completed -- bond is complete, from
  BLE controller
* bt_driver_handle_host_added_bonding -- OS has booted, add bond from
  in-flash database to in-controller database (make sure to kill these in
  bt_driver_stop when reinitting)
* bt_driver_handle_host_removed_bonding -- user requested bond remove from
  UI
* bt_driver_cb_handle_create_bonding -- controller has exchanged keys and
  would like OS to store a bond in flash

### Pebble Pairing Service

Pebble Pairing Service is an internal GATT service managed not by the OS but
by the controller (done in firmware on Dialog, obviously done as a Bluetopia
client on TI).  Implicit in this API is an init that actually sets up the
Pebble Pairing Service, called within BLE stack!

See at least one implementation:
https://github.com/pebble-dev/RebbleOS/blob/master/hw/drivers/nrf52_bluetooth/nrf52_bluetooth_ppogatt.c#L352-L417

* bt_driver_pebble_pairing_service_handle_status_change
* bt_driver_pebble_pairing_service_handle_gatt_mtu_change
* bt_driver_cb_pebble_pairing_service_handle_connection_parameter_write
* bt_driver_cb_pebble_pairing_service_handle_ios_app_termination_detected

### GATT server / client shim

* bt_driver_gatt_respond_read_subscription
* bt_driver_gatt_send_changed_indication
* bt_driver_gatt_start_discovery_range
* bt_driver_gatt_stop_discovery
* bt_driver_gatt_handle_discovery_abandoned
* bt_driver_cb_gatt_client_discovery_handle_indication
* bt_driver_cb_gatt_client_discovery_complete
* bt_driver_cb_gatt_client_operations_handle_response
* bt_driver_cb_gatt_service_changed_server_confirmation
* bt_driver_cb_gatt_service_changed_server_subscribe
* bt_driver_cb_gatt_service_changed_server_read_subscription
* bt_driver_cb_gatt_client_discovery_handle_service_changed
* bt_driver_gatt_write_without_response
* bt_driver_gatt_write
* bt_driver_gatt_read
* bt_driver_cb_gatt_handle_connect
* bt_driver_cb_gatt_handle_disconnect
* bt_driver_cb_gatt_handle_mtu_update
* bt_driver_cb_gatt_handle_notification
* bt_driver_cb_gatt_handle_indication
* bt_driver_cb_gatt_handle_buffer_empty

### Heart rate monitor

The controller implements the GATT functionality for the heart rate monitor,
rather than the OS.  Not implemented on TI.

* bt_driver_is_hrm_service_supported
* bt_driver_cb_hrm_service_update_subscription
* bt_driver_hrm_service_handle_measurement

### Scanning

Scanning was only used by APIs that never became public.

* bt_driver_start_le_scan
* bt_driver_stop_le_scan
* bt_driver_cb_le_scan_handle_report

### Bluetooth Classic

These apply only to Bluetooth Classic and are no-ops on BLE.

* bt_driver_classic_disconnect
* bt_driver_classic_is_connected
* bt_driver_classic_copy_connected_address
* bt_driver_classic_copy_connected_device_name
* bt_driver_classic_update_connectability
* bt_driver_reconnect_pause
* bt_driver_reconnect_resume
* reconnect_set_interval
* bt_driver_reconnect_try_now
* bt_driver_reconnect_reset_interval
* bt_driver_reconnect_notify_platform_bitfield
* bt_driver_le_pairability_set_enabled -- not implemented even on Dialog
* bt_driver_classic_pairability_set_enabled
* sys_app_comm_get_sniff_interval

### BLE advertisement bug workaround APIs.

Bluetopia was buggy.  These APIs should be no-ops with a driver that doesn't
need to be repeatedly kicked.

* bt_driver_advert_is_connectable -- true
* bt_driver_advert_client_has_cycled -- true
* bt_driver_advert_client_set_cycled
* bt_driver_advert_should_not_cycle -- false

### Analytics

* bt_driver_analytics_get_connection_quality
* bt_driver_analytics_collect_ble_parameters
* bt_driver_analytics_external_collect_chip_specific_parameters
* bt_driver_analytics_external_collect_bt_chip_heartbeat
* bt_driver_analytics_get_conn_event_stats
* bluetooth_analytics_handle_ble_pairing_request (callback)


### Factory test mode / debugging.

Implemented both by classic and BLE.  Used for factory test.  Probably
needs to exist for production test in the future, but 

* bt_driver_test_selftest -- implemented
* bt_driver_test_mfi_chip_selftest -- not implemented on LE-only
* hc_endpoint_logging_set_level -- not implemented on TI
* hc_endpoint_logging_get_level -- not implemented on TI
* bt_driver_core_dump -- not implemented on TI
* bt_driver_test_start
* bt_driver_test_enter_hci_passthrough
* bt_driver_test_handle_hci_passthrough_character
* bt_driver_test_enter_rf_test_mode
* bt_driver_test_set_spoof_address
* bt_driver_test_stop

Some drivers also have:

* bt_driver_start_unmodulated_tx
* bt_driver_stop_unmodulated_tx
