# nRF54L CRACEN

`drivers/crypto/nrf_cracen.c` (`CONFIG_CRYPTO_NRF_CRACEN`, default on the
nRF54L) drives the cipher and hash engine of the CRACEN for SHA-256 and
AES-128/192/256 in ECB mode. The CRACEN core (`CONFIG_MFD_NRF_CRACEN`) turns
the engine on for each request, and the random number generator of the same
block on for {doc}`its driver <../rng>`.

The engine reads and writes memory through its own scatter-gather DMA. The
driver copies data, the key and the chaining value through buffers in RAM,
so callers may pass any buffer, in RRAM or at any alignment. Every request,
at most four SHA-256 blocks or eight AES blocks, runs with interrupts locked,
loading the key or the chaining value each time, so the engine needs no
owner. That works before the scheduler starts and from the interrupts
`pbl_irq_lock()` masks, and bounds the interrupt latency it adds to a few
microseconds. More urgent interrupts must not use it.

## Performance

At 128 MHz a request costs about 800 cycles of setup. SHA-256 then takes
about 330 cycles per block against about 4100 for mbedTLS, and AES-128
about 160 per block against about 1500. In cycles per call (`crypto bench`,
typical of three runs):

| Bytes | SHA-256 hw | SHA-256 sw | AES-128 enc hw | AES-128 enc sw | AES-128 dec hw | AES-128 dec sw |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 16 | | | 960 | 1690 | 970 | 1710 |
| 32 | | | 1010 | 3250 | 1120 | 3480 |
| 64 | 1070 | 4540 | 1170 | 6130 | 1180 | 6680 |
| 256 | 1460 | 16960 | 2710 | 23950 | 2790 | 26770 |
| 1024 | 5600 | 66670 | 10580 | 93980 | 10600 | 107000 |
| 4096 | 22090 | 265390 | 41700 | 376780 | 41860 | 428550 |

The hardware wins from the first block of either algorithm, so an
application may lower `CONFIG_CRYPTO_AES_HW_MIN_LEN` to 16.
