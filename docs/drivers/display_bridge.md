# Display bridge

A display bridge is what a display controller sends its pixels through on the
way to the panel, found at the output port of the controller, as a MIPI DSI
host. To a panel with its own frame memory, the controller has the bridge send
a frame on each refresh, timed by the tearing effect of the panel, which the
bridge reports.

| | |
| --- | --- |
| Directory | `drivers/display_bridge` |
| Header | `pbl/drivers/display_bridge.h` |
| Kconfig | `CONFIG_DISPLAY_BRIDGE`, selected by the drivers that use it |
| API | {apidoc}`API reference <drivers_display_bridge>` |

## Drivers

| Driver | Compatible | Kconfig |
| --- | --- | --- |
| ST STM32 DSI host | `st,stm32-dsi` | `CONFIG_MIPI_DSI_STM32` |
