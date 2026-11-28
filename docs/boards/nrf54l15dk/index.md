# Nordic nRF54L15 DK (nrf54l15dk)

## Overview

:::{div} board-card

```{thumbnail} images/nrf54l15dk.webp
```

:Board: `nrf54l15dk/cpuapp`, `nrf54l15dk/cpuflpr`
:SoC: Nordic nRF54L15
:::

The nRF54L15 DK is the Nordic development kit for the nRF54L15. Each core is a
board of its own: `nrf54l15dk/cpuapp` for the Arm application core, which runs
as secure code, and `nrf54l15dk/cpuflpr` for the FLPR, the RISC-V coprocessor.

## Hardware

- **SoC**: [Nordic nRF54L15](https://www.nordicsemi.com/Products/nRF54L15),
  with an Arm Cortex-M33 with FPU at 128 MHz, an RV32EMC coprocessor (the
  FLPR) at 128 MHz, an
  integrated Bluetooth Low Energy radio, 1524 KiB RRAM and 256 KiB RAM
- **Clocks**: 32 MHz and 32.768 kHz crystals, on the internal load
  capacitors
- **Power**: VREGMAIN runs from its DC/DC converter, whose inductor is fitted
- **Buttons**: four user buttons, Button 0 to 3 as keys 0 to 3, Button 0 also
  waking the board from power-off, and a reset button
- **LEDs**: four green user LEDs, LED0 the status LED
- **Flash**: 8 MiB Macronix MX25R6435F SPI NOR, on SPIM00 (P2.01 SCK,
  P2.02 MOSI, P2.04 MISO, P2.05 CS) at 8 MHz
- **RRAM**: the RRAM of the SoC is a second flash, which needs no erase;
  data goes in partitions outside `code`, which the MPU keeps read-only
- **Crypto**: the CRACEN of the SoC, for random numbers, SHA-256 and AES
- **Debug**: SEGGER J-Link OB interface MCU, with two virtual COM ports, on
  UARTE20, P1.04 (TX) and P1.05 (RX), and on UARTE30, P0.00 (TX) and P0.01
  (RX), which also powers the kit

The radio runs the Nordic SoftDevice Controller from the nonfree libraries,
under the NimBLE host.

The FLPR firmware is stored in the last 96 KiB of the RRAM, and runs from the
top 96 KiB of the RAM, code and data together, with UARTE30 for its console:
from the RAM, the FLPR runs over twice as fast as from the RRAM. Once it has
set up the clocks, the application core copies it there and starts it
(`nordic,auto-boot`), when its part of the RRAM holds an image. The
application core owns the rest of the kit, 160 KiB of RAM with it.

## Flashing

The board embeds a J-Link, through which the `nrfutil` runner flashes it.
