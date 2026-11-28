# pblprog-sifli

`pblprog-sifli` is the Pebble programmer for the SiFli SF32LB52 boards. It
is a USB to serial adapter that connects to the watch's B2B
(board-to-board) connector through a flex cable, and gives the host the
watch's debug UART, a reset line and, optionally, power to charge it.

:Boards: {doc}`obelix <../boards/obelix/index>`, {doc}`getafix <../boards/getafix/index>`
:Host interface: USB-C: serial port (WCH CH343P)
:Target interfaces: Debug UART, reset, VBUS
:Runners: `sftool`, `probe-rs`
:Design files: [coredevices/pblprog-sifli](https://github.com/coredevices/pblprog-sifli) (MIT)

## Versions

There are two versions of the board. Confusingly, we have an older board
also called rev.1, so go by the colour and the silkscreen, not the revision
number:

| | Current board (R1) | Older board (rev.1) |
| --- | --- | --- |
| Colour | Green | Blue |
| Silkscreen | `pebble` logo, `pblprog-sifli-board r1` | `pblprog-sifli rev.1`, no logo |
| Reset button | `RST` | `RESET` |
| Charge switch | `<<CHG` | `ON<-CHG->OFF` |
| Flex cable | `pblprog-sifli-flex` | `pblprog-sifli debug connect fpc V1.0` |

The rest of this page describes the current green R1 board.

### R1 (current, green)

The current board, marked `pblprog-sifli-board r1`.

::::{grid} 1 1 2 2
:gutter: 2

:::{grid-item}
```{figure} images/pblprog-sifli-board.webp
pblprog-sifli-board
```
:::

:::{grid-item}
```{figure} images/pblprog-sifli-flex.webp
pblprog-sifli-flex
```
:::

::::

### rev.1 (older, blue)

The older board, marked `pblprog-sifli rev.1`, with the
`pblprog-sifli debug connect fpc V1.0` flex cable. Despite the similar
name, it is not the same board as the green R1.

::::{grid} 1 1 2 2
:gutter: 2

:::{grid-item}
```{figure} images/pblprog-sifli-rev1-board.webp
pblprog-sifli rev.1
```
:::

:::{grid-item}
```{figure} images/pblprog-sifli-rev1-connected.webp
pblprog-sifli rev.1 connected to a watch
```
:::

:::{grid-item}
:columns: 12
```{figure} images/pblprog-sifli-rev1-back.webp
pblprog-sifli rev.1 connected to a watch, back of the board
```
:::

:::{grid-item}
:columns: 12
```{figure} images/pblprog-sifli-rev1-flex.webp
pblprog-sifli debug connect fpc V1.0
```
:::

::::

## Features

- USB-C connector, with an overcurrent fuse and ESD protection
- WCH CH343P USB to UART converter. Its I/O is powered by the watch (`VIO`),
  so the UART always runs at the watch's logic level.
- Reset: the `RST` button, or the serial port's RTS line, resets the watch.
  RTS reaches the reset line through the `JP1` solder jumper, bridged by
  default; cut it to keep the host from resetting the watch.
- Charge switch: with `CHG` on, the USB 5 V feeds the watch's `VBUS` and
  charges it while connected.
- Molex SlimStack 505004-0812 B2B receptacle, for the flex cable

### Connector

| Pin | Signal | Description |
| --- | --- | --- |
| 1 | | Not connected |
| 2 | `RESET` | Watch reset, active low |
| 3 | `UART_TXD` | Programmer transmit, to the watch |
| 4 | `GND` | Ground |
| 5 | | Not connected |
| 6 | `VIO` | Watch I/O voltage, powers the UART |
| 7 | `VBUS` | 5 V to the watch, through the `CHG` switch |
| 8 | `UART_RXD` | Programmer receive, from the watch |

### Flex cable

`pblprog-sifli-flex` has a Molex SlimStack 505006-0812 plug on each end and
connects the programmer to the watch. Its pins are reversed end to end
(1–4, 2–3, 5–8, 6–7), matching the orientation of the two connectors.
