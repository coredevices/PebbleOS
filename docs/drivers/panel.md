# Panel

A panel is the glass behind a display controller: it owns its supplies and
drive signals, and describes the timing and bus format the controller has to
generate. The controller brings it up and down in steps around its own.

| | |
| --- | --- |
| Directory | `drivers/panel` |
| Header | `pbl/drivers/panel.h` |
| Kconfig | `CONFIG_PANEL`, selected by the drivers and code that use it |
| API | {apidoc}`API reference <drivers_panel>` |

## Drivers

| Driver | Compatible | Kconfig |
| --- | --- | --- |
| Memory-in-pixel panels | `sharp,ls013b7dd02`, `jdi,lpm015m135a` | `CONFIG_PANEL_JDI_MIP` |
| ChipOne CO5300 AMOLED panels | `chipone,co5300` | `CONFIG_PANEL_CO5300` |
