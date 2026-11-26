# MIPI DSI

A MIPI DSI host is the controller a panel is wired to over MIPI DSI. The
panel's driver attaches to it with its lanes, pixel format and mode, then sends
its commands through it, those of its power sequence and settings such as
brightness. A host in command mode is also the
{doc}`display bridge <display_bridge>` its display controller sends frames
through.

| | |
| --- | --- |
| Directory | `drivers/mipi_dsi` |
| Header | `pbl/drivers/mipi_dsi.h` |
| Kconfig | `CONFIG_MIPI_DSI`, selected by the drivers and code that use it |
| API | {apidoc}`API reference <drivers_mipi_dsi>` |

## Drivers

| Driver | Compatible | Kconfig |
| --- | --- | --- |
| ST STM32 DSI host | `st,stm32-dsi` | `CONFIG_MIPI_DSI_STM32` |
