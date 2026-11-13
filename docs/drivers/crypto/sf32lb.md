# SF32LB

`drivers/crypto/sf32lb.c` (`CONFIG_CRYPTO_SF32LB`, default on SF32LB52)
drives the AES and hash accelerator for SHA-256 and AES-128/192/256 in ECB
mode.

The engine reads and writes memory through its own bus master. The driver
copies data through a buffer in SRAM, so callers may pass any buffer, in
flash, PSRAM or at any alignment, and the buffer owns its cache lines.
Every request, at most four SHA-256 blocks or eight AES blocks, runs with
interrupts locked and the clock enabled, loading the key or the chaining
value each time, so the engine needs no owner. That works before the
scheduler starts and from the interrupts `pbl_irq_lock()` masks, and bounds
the interrupt latency it adds to a few microseconds. More urgent interrupts
must not use it.

The hash unit takes the mode and IV selection for an IV load from before
the write that triggers it, so the driver selects them in a write of their
own first.

## Performance

At 240 MHz a request costs about 1500 cycles of setup. SHA-256 then takes
about 500 cycles per block against about 4100 for mbedTLS, and AES-128
about 250 per block against about 950. In cycles per call (`crypto bench`,
typical of three runs):

| Bytes | SHA-256 hw | SHA-256 sw | AES-128 enc hw | AES-128 enc sw | AES-128 dec hw | AES-128 dec sw |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 16 | | | 2000 | 2500 | 2200 | 1700 |
| 32 | | | 1550 | 3600 | 1550 | 2550 |
| 64 | 2100 | 5000 | 1700 | 5000 | 2000 | 4300 |
| 256 | 2300 | 17200 | 4300 | 15700 | 4600 | 15300 |
| 1024 | 8700 | 67100 | 15200 | 61800 | 15800 | 62900 |
| 4096 | 35900 | 267000 | 67600 | 244700 | 68700 | 241200 |

A single AES block is a tie within the noise, so AES goes to hardware from
two blocks on; SHA-256 wins from its first block.

The `CONFIG_CRYPTO_SHA256_HW_MIN_LEN` (64) and `CONFIG_CRYPTO_AES_HW_MIN_LEN`
(32) defaults come from these crossovers.
