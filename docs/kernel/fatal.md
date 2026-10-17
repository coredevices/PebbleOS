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
The firmware's, in `fw/kernel/fault_handling.c`, records an assertion reboot
reason, saves a core dump and reboots, or only kills the app when the error was
raised by unprivileged code. Unit tests replace the whole of `kernel/fatal.c`
with `tests/stubs/default/fatal.c`, so `cl_assert_panic()` can catch a panic.
