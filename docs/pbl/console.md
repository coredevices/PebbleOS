# `pbl console`

Attach to the firmware's console: over serial on a watch, over TCP to a
running emulator, or to the native simulator under {doc}`debug`, which
moves its console off the terminal.

```shell
pbl console                       # the emulator, or the lab's watch
pbl console --tty /dev/ttyACM0    # a watch
```

On a watch, the port defaults to that of the {doc}`lab's <lab>` watch for the
build, and the baud rate to the build's `CONFIG_CONSOLE_BAUDRATE`.

A console on the firmware's own USB serial port (`CONFIG_USB_CDC_ACM`, the
port having the build's USB vendor and product IDs) keeps what it printed
until the host opens the port, and sends it then. `pbl console` keeps those
bytes, where the serial library would throw away what arrives as the port
opens, so the boot banner and the logs since boot show up, also each time
`--reconnect` opens the port again. On other ports, such as a UART behind a
USB bridge, it still drops what arrived before it opened them.

Send `help` to list the console commands. Firmware log messages are hashed
at compile time, so the binary only contains a hash and the arguments; the
console dehashes them on the fly using the dictionary the build generates
at `build/log.dict.json`. {doc}`../development/debugging/index` covers the flash log buffer and
coredumps.

## Options

```{pbl-command} console
```
