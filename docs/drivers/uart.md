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
| POSIX UART | — | `CONFIG_UART_POSIX` |
