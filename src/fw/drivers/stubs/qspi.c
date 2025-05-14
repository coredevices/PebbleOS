#include "drivers/qspi.h"
#include "drivers/qspi_definitions.h"

#include "drivers/flash/qspi_flash.h"
#include "drivers/flash/qspi_flash_definitions.h"

#include "drivers/flash/flash_impl.h"

status_t flash_impl_set_nvram_erase_status(bool is_subsector,
                                           FlashAddress addr) {
  return S_SUCCESS;
}

status_t flash_impl_clear_nvram_erase_status(void) {
  return S_SUCCESS;
}

status_t flash_impl_get_nvram_erase_status(bool *is_subsector,
                                           FlashAddress *addr) {
  // XXX
  return S_FALSE;
}
