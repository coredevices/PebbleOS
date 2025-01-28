#include "nrf_sdh_ble.h"

#include <bluetooth/id.h>
#include <bluetooth/bluetooth_types.h>

#include <string.h>


void bt_driver_id_set_local_device_name(const char device_name[BT_DEVICE_NAME_BUFFER_SIZE]) {
}

void bt_driver_id_copy_local_identity_address(BTDeviceAddress *addr_out) {
  ble_gap_addr_t addr;
  sd_ble_gap_addr_get(&addr);
  memcpy(addr_out, addr.addr, 6);
}

void bt_driver_set_local_address(bool allow_cycling,
                                 const BTDeviceAddress *pinned_address) {
}

void bt_driver_id_copy_chip_info_string(char *dest, size_t dest_size) {
  strncpy(dest, "nRF52840", dest_size);
}

bool bt_driver_id_generate_private_resolvable_address(BTDeviceAddress *address_out) {
  *address_out = (BTDeviceAddress) {};
  return true;
}
