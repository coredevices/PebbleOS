# Mailbox

A mailbox controller passes messages, or just doorbells, between processors
over channels. A consumer takes its channel from a devicetree `mboxes` entry.

| | |
| --- | --- |
| Directory | `drivers/mbox` |
| Header | `pbl/drivers/mbox.h` |
| Kconfig | `CONFIG_MBOX`, selected by the drivers and code that use it |
| API | {apidoc}`API reference <drivers_mbox>` |

## Drivers

| Driver | Compatible | Kconfig |
| --- | --- | --- |
| SiFli SF32LB mailbox | `sifli,sf32lb52-mailbox` | `CONFIG_MBOX_SF32LB` |
| STM32H7 hardware semaphores | `st,stm32-hwspinlock` | `CONFIG_MBOX_STM32_HSEM` |
