#include "firmware_storage.h"

#include "drivers/flash.h"
#include <drivers/dbgserial.h>

FirmwareDescription firmware_storage_read_firmware_description(uint32_t firmware_start_address) {
  FirmwareDescription firmware_description;
  flash_read_bytes((uint8_t*)&firmware_description, firmware_start_address,
                   sizeof(FirmwareDescription));
  dbgserial_print("Firmware length: ");
  dbgserial_print_hex(firmware_description.firmware_length);
  dbgserial_newline();
  dbgserial_print("Checksum: ");
  dbgserial_print_hex(firmware_description.checksum);
  dbgserial_newline();
  return firmware_description;
}

bool firmware_storage_check_valid_firmware_description(FirmwareDescription* desc) {
  return desc->description_length == sizeof(FirmwareDescription);
}
