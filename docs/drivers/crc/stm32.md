# STM32

`drivers/crc/stm32.c` (`CONFIG_CRC_STM32`, default with an enabled
`st,stm32f7-crc` node) drives the CRC unit of the STM32F7 and later, as on
the STM32H7. As the SF32LB driver does, it programs the polynomial, initial
value and reflection for every burst of up to 256 bytes under
`pbl_irq_lock()`, carrying the running value between bursts, and opens the
clock gate of the unit for the length of a call.

The unit takes each 32-bit word most significant bit first: the legacy
checksum feeds it the words as they are, and CRC-32 reverses the bits of the
whole word, so that its bytes go in in memory order.

## Performance

On the STM32H747 at 400 MHz, a call costs about 1000 cycles of setup, most
of it opening and closing the clock gate, then about 3.3 cycles per byte
against about 17 for the software tables, the unit being in the D3 domain,
behind two bus bridges. It breaks even around 70 bytes and is about 4 times
faster on 1 KiB, which sets the `CONFIG_CRC_HW_MIN_LEN` default of 128 with
this driver.

## Self-test

On first use the driver checks both algorithms against fixed test vectors,
in one burst and resumed across two, and disables itself on any mismatch,
logging an error. The software path then serves every request.
