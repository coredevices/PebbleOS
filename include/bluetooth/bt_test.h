/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <bluetooth/bluetooth_types.h>
#include <bluetooth/hci_types.h>

#include <stdbool.h>

void bt_driver_test_start(void);

void bt_driver_test_enter_hci_passthrough(void);

void bt_driver_test_handle_hci_passthrough_character(char c, bool *should_context_switch);

bool bt_driver_test_enter_rf_test_mode(void);

void bt_driver_test_set_spoof_address(const BTDeviceAddress *addr);

void bt_driver_test_stop(void);

bool bt_driver_test_selftest(void);

bool bt_driver_test_mfi_chip_selftest(void);

void bt_driver_le_transmitter_test(
    uint8_t tx_channel, uint8_t tx_packet_length, uint8_t packet_payload);
void bt_driver_le_test_end(void);
void bt_driver_le_receiver_test(uint8_t rx_channel);

typedef void (*BTDriverResponseCallback)(HciStatusCode status, const uint8_t *payload);
void bt_driver_register_response_callback(BTDriverResponseCallback callback);

void bt_driver_start_unmodulated_tx(uint8_t tx_channel);
void bt_driver_stop_unmodulated_tx(void);

typedef enum BtlePaConfig {
  BtlePaConfig_Disable,
  BtlePaConfig_Enable,
  BtlePaConfig_Bypass,
  BtlePaConfigCount
} BtlePaConfig;
void bt_driver_le_test_pa(BtlePaConfig option);

typedef enum BtleCoreDump {
  BtleCoreDump_UserRequest,
  BtleCoreDump_ForceHardFault,
  BtleCoreDump_Watchdog,
  BtleCoreDumpCount
} BtleCoreDump;
void bt_driver_core_dump(BtleCoreDump type);

void bt_driver_send_sleep_test_cmd(bool force_ble_sleep);
