# Console

The console is one serial port, the board's `CONSOLE_UART`, at
`CONFIG_CONSOLE_BAUDRATE`. It is plain text, so any terminal works: log lines
print above the shell's prompt, and the shell is always listening. Its code is
in `subsys/console/`, and `tools/libs/pbl-console` is the host side.

Release firmware has no console (`CONFIG_CONSOLE`), unless it is manufacturing
firmware: no shell, no serial logs, and nothing listening on the port.

Use `pbl console` to get log lines dehashed on builds that hash them; it is
otherwise a plain terminal.

## Frames

Binary data travels in frames between the text lines:

```
ESC _ <channel> <base64(seq, payload, crc32)> ESC \
```

The channel is one character. The CRC-32 is `pbl_crc32()` of the channel, the
sequence number and the payload, little-endian. A payload is at most
`PBL_CONSOLE_FRAME_MTU` (512) bytes. The firmware only sends frames once a host has
started using them, so a terminal does not see them. Anything received that
does not look like a frame goes back to being text, which keeps a stray escape
sequence from wedging the console.

| Channel | Use |
|---|---|
| `A` | Acknowledgment, of the sequence number in its payload |
| `P` | The Pebble protocol |
| `B` | Flash imaging |

The host sends one frame at a time, and repeats it after a second without
acknowledgment. The firmware handles a frame on SysWorkq and acknowledges it
afterwards, so the acknowledgment says that it was handled, and whatever the
firmware sent in response comes before it. A repeated sequence number is
acknowledged without handling the frame again, except 0, for frames that do no
harm if handled twice.

The firmware sends with sequence number 0 and does not wait for the host. A
damaged frame from the firmware leaves a hole in the stream, so the host ends
the session and opens another. Log messages from interrupts wait in a ring
buffer until KernelMain can write them, to stay out of the middle of a frame.

## Pebble protocol

The payload starts with an opcode: 1 for data, which is the protocol's byte
stream, 2 to open a session and 3 to close it. The firmware answers open and
close with the same opcode, and data without a session with close, as after a
restart.

## Flash imaging

`pbl image_resources` and `pbl image_recovery` use this channel where sftool is
not used. The firmware enables it with the console (`CONFIG_CONSOLE_IMAGING`). The payload starts with an opcode, and integers are little-endian:

| Opcode | Arguments | Response |
|---|---|---|
| 1, erase | address, length (u32) | 128 when erased |
| 2, write | address (u32), data | none; the acknowledgment says it is written |
| 3, CRC-32 | address, length (u32) | 130, CRC-32 (u32) |
| 4, query a region | region (u8) | 131, address, length (u32) |
| 5, finalize a region | region (u8) | 132 |

Regions are 1 for the recovery firmware and 2 for the resources. Any command
can respond with 193 if it fails.

## Reading from the watch

There is no bulk transfer channel. The Pebble protocol's GetBytes endpoint
serves the newest coredump on any build, and files and flash ranges on
non-release builds, which also reaches the other coredump slots. On the host,
`libpebble2.services.getbytes.GetBytesService` works over this channel as it
does over Bluetooth. Reading RAM takes a debugger.
