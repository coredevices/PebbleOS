# RTT

`subsys/rtt/` implements SEGGER RTT: channels in RAM that a debug probe reads
and writes over SWD while the target runs, with no pins and from the first
instruction. Its public API is `include/pbl/rtt/rtt.h`. It is built with
`CONFIG_RTT`, which its users select, such as the RTT serial port of the
[UART](../drivers/uart.md) class.

The control block is the global `_SEGGER_RTT`, which debug probes find by
that symbol in the ELF. It holds `CONFIG_RTT_NUM_UP` channels to the probe and
`CONFIG_RTT_NUM_DOWN` from it; channel 0 is the terminal. Writes never wait:
one that does not fit is dropped whole. `pbl_rtt_flush()` waits only for a
probe seen reading the channel, and stops waiting for one that stopped.

The probe reads and writes the RAM itself, behind any data cache, and the
offsets it writes share cache lines with the ones the target writes, so cache
maintenance cannot keep them coherent. With a data cache, RTT selects
`CONFIG_KERNEL_NOCACHE` and places the control block and the channel buffers
in the kernel's non-cacheable section ({doc}`../kernel/memory`).

## OpenOCD

`tools/openocd/rtt.cfg` serves channel 0 on a TCP port. On boards with the
OpenOCD runner, `pbl debugserver` and `pbl debug` load it for builds with
`CONFIG_RTT`, with the address of `_SEGGER_RTT` from the build's ELF, and
`pbl console --rtt` opens the port:

```shell
pbl debugserver
pbl console --rtt
```

OpenOCD finds the control block once, when it starts: restart it after
flashing another build.
