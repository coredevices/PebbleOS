#include <bluetooth/bt_driver_advert.h>

void bt_driver_advert_advertising_disable(void) {
}

bool bt_driver_advert_is_connectable(void) {
  return false;
}

bool bt_driver_advert_client_get_tx_power(int8_t *tx_power) {
  return false;
}

void bt_driver_advert_set_advertising_data(const BLEAdData *ad_data) {
}

bool bt_driver_advert_advertising_enable(uint32_t min_interval_ms, uint32_t max_interval_ms,
                                     bool enable_scan_resp) {
  return false;
}

bool bt_driver_advert_client_has_cycled(void) {
  return false;
}

void bt_driver_advert_client_set_cycled(bool has_cycled) {
}

bool bt_driver_advert_should_not_cycle(void) {
  return false;
}
