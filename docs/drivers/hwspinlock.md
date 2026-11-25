# Hardware spinlock

A hardware spinlock serializes processors that share registers or memory,
where an interrupt lock only serializes the code of one. A consumer takes its
lock from a devicetree `hwlocks` entry and holds it, with interrupts locked,
around the accesses it shares. Without an entry, taking it only locks
interrupts. Locks work before their controller is initialized, as for the pins
of an early console.

| | |
| --- | --- |
| Directory | `drivers/hwspinlock` |
| Header | `pbl/drivers/hwspinlock.h` |
| Kconfig | `CONFIG_HWSPINLOCK`, selected by the drivers and code that use it |
| API | {apidoc}`API reference <drivers_hwspinlock>` |

A holder keeps a lock for a few register accesses. A processor waiting for
longer than `CONFIG_HWSPINLOCK_TIMEOUT_US` gives up with `-ETIMEDOUT`: the
holder stopped with it.
