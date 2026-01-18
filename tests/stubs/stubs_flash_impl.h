#pragma once

#include "drivers/flash/flash_impl.h"
#include "system/status_codes.h"

// Stub implementations for flash security register functions
// These are only used by certain platforms but the flash_api.c code
// calls them unconditionally, so we need stubs for tests

status_t flash_impl_read_security_register(uint32_t addr, uint8_t *val) {
  return E_ERROR;
}

status_t flash_impl_security_register_is_locked(uint32_t address, bool *locked) {
  return E_ERROR;
}

status_t flash_impl_erase_security_register(uint32_t addr) {
  return E_ERROR;
}

status_t flash_impl_write_security_register(uint32_t addr, uint8_t val) {
  return E_ERROR;
}

const FlashSecurityRegisters *flash_impl_security_registers_info(void) {
  return NULL;
}
