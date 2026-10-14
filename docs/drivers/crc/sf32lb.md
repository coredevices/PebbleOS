# SF32LB

`drivers/crc/sf32lb.c` (`CONFIG_CRC_SF32LB`, default on SF32LB52) drives
the CRC1 unit. It programs the polynomial, initial value and reflection for
every burst of up to 256 bytes under `pbl_irq_lock()`, carrying the running
value between bursts, so the unit needs no owner. That works before the
scheduler starts and from interrupt context, and bounds the interrupt
latency it adds to a few microseconds.

## Performance

At 240 MHz a call costs about 330 cycles of setup, then about 5.4 cycles per
byte against about 20 for the software tables. The unit breaks even around
24 bytes and is about 3.6 times faster on large buffers, which sets the
`CONFIG_CRC_HW_MIN_LEN` default of 32.

## Self-test

On first use the driver checks both algorithms against fixed test vectors,
in one burst and resumed across two, and disables itself on any mismatch,
logging an error. The software path then serves every request.
