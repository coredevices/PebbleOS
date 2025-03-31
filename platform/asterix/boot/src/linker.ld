MEMORY
{
  /* Retained, 256 bytes */
  RETAINED (rw): ORIGIN = 0x20000000, LENGTH = 256

  FLASH (rx) : ORIGIN = 0x00000000, LENGTH = 0x4000
  FLASH_FW (rx) : ORIGIN = 0x00004000, LENGTH = 0xfc000
  RAM (rw) :  ORIGIN = 0x20000100, LENGTH = 0x3ff00
}

SECTIONS
{
    .retained : ALIGN(4) {
        __retained_start = .;
        *(.retained)
        __retained_end = .;
    } > RETAINED

    .flash_fw : ALIGN(4) {
        __fw_start = .;
    } > FLASH_FW
}

INCLUDE "../../../../third_party/hal_nordic/nrfx/mdk/nrf_common.ld"

