# DMA

A DMA controller moves data between memory and peripherals over numbered
channels. A consumer takes its channel from a devicetree `dmas` entry, sets
the peripheral side up once, and runs one transfer at a time.

| | |
| --- | --- |
| Directory | `drivers/dma` |
| Header | `pbl/drivers/dma.h` |
| Kconfig | `CONFIG_DMA`, selected by the drivers and code that use it |
| API | {apidoc}`API reference <drivers_dma>` |

## Drivers

| Driver | Compatible | Kconfig |
| --- | --- | --- |
| SiFli SF32LB DMA controller | `sifli,sf32lb57-gpdma`, `sifli,sf32lb52-dmac` | `CONFIG_DMA_SF32LB` |
