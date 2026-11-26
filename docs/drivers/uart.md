# UART

A serial port sends bytes polled, and hands received bytes to a callback from
its interrupt while reception is enabled.

| | |
| --- | --- |
| Directory | `drivers/uart` |
| Header | `pbl/drivers/uart.h` |
| Kconfig | `CONFIG_UART`, enabled by the application |
| API | {apidoc}`API reference <drivers_uart>` |

## Drivers

| Driver | Compatible | Kconfig |
| --- | --- | --- |
| Nordic nRF5 UART Controller | `nordic,nrf52832-uarte` | `CONFIG_UART_NRF5` |
| SiFli SF32LB UART Controller | `sifli,sf32lb52-usart` | `CONFIG_UART_SF32LB` |
| ST STM32 UART Controller | `st,stm32h7-uart` | `CONFIG_UART_STM32` |
| QEMU Simple UART | `qemu,pebble-simple-uart` | `CONFIG_UART_QEMU` |
| SEGGER RTT | — | `CONFIG_UART_RTT` |
| POSIX UART | — | `CONFIG_UART_POSIX` |

## SEGGER RTT

The RTT port (`RTT_UART`) is a channel of the [RTT](../subsystems/rtt.md)
control block, which a debug probe reads and writes over SWD while the target
runs: a console needs no pins and prints from the first boot log and from the
fault handler. It has no devicetree node; a board makes it its console with
`CONFIG_UART_RTT` and its `console_uart` role. While reception is enabled it
polls on a timer every `CONFIG_UART_RTT_POLL_MS`, which keeps the system out
of its deepest idle states: a console for debugging, not for battery tests.
