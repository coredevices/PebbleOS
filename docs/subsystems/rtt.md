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

`tools/openocd/rtt.cfg` serves channel 0 on a TCP port, which `pbl console`
opens:

```shell
openocd -f boards/<board>/support/openocd.cfg \
  -c "set RTT_ADDR 0x$(arm-none-eabi-nm build/pebbleos.elf | awk '/ _SEGGER_RTT$/ {print $1}')" \
  -f tools/openocd/rtt.cfg
pbl console --tty socket://localhost:9090
```

OpenOCD finds the control block once, when it starts: restart it after
flashing another build.
