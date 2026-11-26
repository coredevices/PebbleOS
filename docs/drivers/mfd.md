# Multi-function devices

A multi-function device is a chip, or a block of a SoC, that bundles several
functions, such as a PMIC with a charger, GPIOs and regulators. Its driver
handles the core, such as register access, and each function is a child
device of its own class; see {ref}`device-model-mfd`.

| | |
| --- | --- |
| Directory | `drivers/mfd` |
| Kconfig | `CONFIG_MFD`, selected by the drivers and code that use it |

## Drivers

| Driver | Compatible | Kconfig |
| --- | --- | --- |
| Nordic nPM1300 | `nordic,npm1300` | `CONFIG_MFD_NPM1300` |
| Nordic nRF54L CRACEN | `nordic,nrf54l15-cracen` | `CONFIG_MFD_NRF_CRACEN` |
