# Print

`pbl/kernel/print.h` writes plain text for the system's own messages, such as
the boot banner:

```c
pbl_print("*** Booting PebbleOS %s ***\n", PBL_VERSION_TAG);
```

`pbl_print()` formats into a buffer on the stack, cut at one byte less than
`CONFIG_KERNEL_PRINT_BUFFER_SIZE` (128 by default), and hands it to the output
set with `pbl_print_set_output()` before returning. Lines end with the newline
the caller writes. Nothing is filtered or recorded, which is
what {doc}`../subsystems/logging` is for, and it works from any context, before
the scheduler runs too. Without an output, the text is dropped.

The kernel owns `pbl_print()`; the output belongs to whoever drives the
console. With `CONFIG_CONSOLE`, `pbl_console_early_init()` sets the console
port up for polled output during boot and registers it, with newlines sent as
CR LF.
