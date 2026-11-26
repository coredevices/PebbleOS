# Display

A display scans a framebuffer owned by the caller out to a panel: a
controller driving its panel, or a panel with its own memory on a bus. An
update sends a range of rows; the panel's mounting rotation is part of the
display.

| | |
| --- | --- |
| Directory | `drivers/display` |
| Header | `pbl/drivers/display.h` |
| Kconfig | `CONFIG_DISPLAY`, enabled by the application |
| Role | `DISPLAY` |
| Sample | {doc}`../samples/drivers/display` |
| API | {apidoc}`API reference <drivers_display>` |

## Drivers

| Driver | Compatible | Kconfig |
| --- | --- | --- |
| SiFli SF32LB LCD controller | `sifli,sf32lb52-lcdc` | `CONFIG_DISPLAY_SF32LB_LCDC` |
| SiFli SF32LB57 LCD controller | `sifli,sf32lb57-lcdc` | `CONFIG_DISPLAY_SF32LB57_LCDC` |
| ST STM32 LTDC | `st,stm32-ltdc` | `CONFIG_DISPLAY_STM32_LTDC` |
| Sharp memory LCDs | `sharp,ls010b7dh04`, `sharp,ls011b7dh03`, `sharp,ls012b7dd01`, `sharp,ls013b7dh01`, `sharp,ls013b7dh03`, `sharp,ls013b7dh05`, `sharp,ls027b7dh01`, `sharp,ls027b7dh01a`, `sharp,ls044q7dh01` | `CONFIG_DISPLAY_SHARP_MEMORY` |
| QEMU Framebuffer Display | `qemu,pebble-display` | `CONFIG_DISPLAY_QEMU` |
| SDL window | — | `CONFIG_DISPLAY_SDL` |
