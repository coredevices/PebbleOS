# Interrupt controller

A secondary interrupt controller, such as a GPIO port or a peripheral that
funnels several sources into one SoC line, provides interrupt lines. A
consumer takes its line from the devicetree, requests it once, and enables it
while it wants interrupts. SoC lines themselves are bound at build time and
are not part of this class.

| | |
| --- | --- |
| Directory | `drivers/intc` |
| Header | `pbl/drivers/intc.h` |
| Kconfig | `CONFIG_INTC`, selected by the drivers and code that use it |
| API | {apidoc}`API reference <drivers_intc>` |

## Drivers

| Driver | Compatible | Kconfig |
| --- | --- | --- |
| GPIO ports |  | {doc}`gpio` |
