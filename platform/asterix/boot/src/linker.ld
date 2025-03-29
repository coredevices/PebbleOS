MEMORY
{
  /* Retained, 80 bytes */
  RETAINED (rw): ORIGIN = 0x20000000, LENGTH = 80

  /* Softdevice takes 156K of FLASH, 8800 bytes of RAM */
  FLASH (rx) : ORIGIN = 0x00000000, LENGTH = 0x4000
  RAM (rw) :  ORIGIN = 0x20000100, LENGTH = 0x3dd50
}

SECTIONS
{
    .retained : ALIGN(4) {
        __retained_start = .;
        *(.retained)
        __retained_end = .;
    } > RETAINED
}

INCLUDE "../../../../third_party/hal_nordic/nrfx/mdk/nrf_common.ld"

