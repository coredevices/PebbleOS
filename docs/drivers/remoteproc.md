# Remote processor

A remote processor runs its own firmware, which its driver boots and halts.
Clients reach that firmware through resources such as mailbox channels and
shared memory.

| | |
| --- | --- |
| Directory | `drivers/remoteproc` |
| Header | `pbl/drivers/remoteproc.h` |
| Kconfig | `CONFIG_REMOTEPROC`, selected by the drivers and code that use it |
| API | {apidoc}`API reference <drivers_remoteproc>` |

## Drivers

| Driver | Compatible | Kconfig |
| --- | --- | --- |
| SiFli SF32LB LCPU | `sifli,sf32lb57-lcpu`, `sifli,sf32lb52-lcpu` | `CONFIG_REMOTEPROC_SF32LB` |
| STM32H7 Cortex-M4 | `st,stm32h747-m4` | `CONFIG_REMOTEPROC_STM32H7_CM4` |
