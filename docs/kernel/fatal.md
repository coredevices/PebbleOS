# Fatal errors and assertions

`pbl/kernel/fatal.h` and `pbl/kernel/assert.h` are how firmware code, the
kernel included, stops the system on an unrecoverable error:

- `pbl_panic()` always stops; `pbl_panic(fmt, ...)` adds a message;
- `PBL_ASSERT(expr, fmt, ...)` stops with a message when `expr` is false;
- `PBL_ASSERTN(expr)` stops without one, the cheapest check.

Assertions are always enabled. Messages must be string literals: with
`CONFIG_LOG_HASHED` they become log tokens like any `PBL_LOG`, so they cost no
flash, and a call site without a message is reduced to its return address.

`kernel/fatal.c` logs the error and the address of the code that raised it,
then calls `pbl_fatal_error_handler()`, the one hook the platform implements.
The firmware's, in `pebble/kernel/fault_handling.c`, records an assertion reboot
reason, saves a {doc}`core dump <../subsystems/coredump>` and reboots, or only
kills the app when the error was raised by unprivileged code. Unit tests replace the whole of `kernel/fatal.c`
with `tests/stubs/default/fatal.c`, so `cl_assert_panic()` can catch a panic.

## CPU faults

The arch handles the CPU's fault exceptions (`kernel/arch/arm/fault.c` on ARM):
it logs the fault status, the stacked registers and the thread, tells a stack
overflow from other faults through the MPU stack guards and the stack limit
registers, and hands a portable `struct pbl_fault_info` to one of two hooks:

- `pbl_fault_thread_handler()` when an unprivileged thread faulted on its own
  stack: the arch leaves the faulting code for the hook, on the same thread, now
  privileged, moving its stack pointer out of the guard first on an overflow;
- `pbl_fault_fatal_handler()` otherwise, from the exception. The arch passes the
  fault to `pbl_fault_fatal_record()` before logging anything, so a fault or
  hang while logging still leaves a record behind.

The firmware's hooks, also in `pebble/kernel/fault_handling.c`, kill the app or
worker that faulted, or record a hard fault or stack overflow reboot reason and
reboot.
