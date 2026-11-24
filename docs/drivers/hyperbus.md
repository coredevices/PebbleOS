# HyperBus

A HyperBus controller drives one HyperBus memory and maps it. The driver of
the memory reaches its registers and sets the bus up through this class,
without knowing the controller.

| | |
| --- | --- |
| Directory | `drivers/hyperbus` |
| Header | `pbl/drivers/hyperbus.h` |
| Kconfig | `CONFIG_HYPERBUS`, selected by the drivers and code that use it |
| API | {apidoc}`API reference <drivers_hyperbus>` |

## Drivers

| Driver | Compatible | Kconfig |
| --- | --- | --- |
| SiFli SF32LB MPI | `sifli,sf32lb57-mpi`, `sifli,sf32lb52-mpi` | `CONFIG_HYPERBUS_SF32LB_MPI` |
