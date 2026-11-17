# SiFli 1.85" AMOLED module (sifli_lcm_1p85)

## Overview

:::{div} board-card

```{image} images/sifli_lcm_1p85.webp
:alt: SiFli 1.85" AMOLED module, front and back
```

:Add-on: `sifli_lcm_1p85`
:Connector: SiFli quad SPI LCD (`sifli,lcd-qspi-connector`)
:Display: 390x450 AMOLED
:Touch: capacitive, one contact
:::

The SiFli display module for its development kits: a 1.85" rectangular
AMOLED with rounded corners, written over quad SPI, and a capacitive
touchscreen on I2C. It plugs into the quad SPI LCD connector of the kits
through a flat cable, whose length and pin count depend on the kit.

## Hardware

- **Panel**: ZC-A1D85W-010, 390x450 AMOLED, 800 cd/m², on a ChipOne CO5300
  driver with its frame memory, written in RGB565 over quad SPI
- **Touch**: FocalTech FT6146 at I2C address `0x38`, with its interrupt and
  reset lines on the connector
- **Power**: BV6802W AMOLED supply, from the 3.3 V of the connector
