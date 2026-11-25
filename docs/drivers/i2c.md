# I2C

An I2C bus carries transfers to peripherals, each a list of read and write
messages sent as one transaction. Transfers block the caller and are
serialized per bus.

| | |
| --- | --- |
| Directory | `drivers/i2c` |
| Header | `pbl/drivers/i2c.h` |
| Kconfig | `CONFIG_I2C`, selected by the drivers and code that use it |
| API | {apidoc}`API reference <drivers_i2c>` |

## Drivers

| Driver | Compatible | Kconfig |
| --- | --- | --- |
| Nordic nRF5 I2C Controller | `nordic,nrf52832-twim` | `CONFIG_I2C_NRF5` |
| SiFli SF32LB I2C Controller | `sifli,sf32lb52-i2c` | `CONFIG_I2C_SF32LB` |
