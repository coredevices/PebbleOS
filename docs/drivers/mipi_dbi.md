# MIPI DBI

A MIPI DBI host is the controller a panel with its own frame memory is wired
to. The panel's driver sends its commands through it, those of its power
sequence and settings such as brightness, and the host sends the pixels.

| | |
| --- | --- |
| Directory | `drivers/mipi_dbi` |
| Header | `pbl/drivers/mipi_dbi.h` |
| Kconfig | `CONFIG_MIPI_DBI`, selected by the drivers and code that use it |
| API | {apidoc}`API reference <drivers_mipi_dbi>` |

## Drivers

| Driver | Compatible | Kconfig |
| --- | --- | --- |
| SiFli SF32LB57 LCD controller | `sifli,sf32lb57-lcdc` | `CONFIG_DISPLAY_SF32LB57_LCDC` |
