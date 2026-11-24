# OTP

OTP slots hold write-once strings, such as serial numbers and hardware
revisions.

| | |
| --- | --- |
| Directory | `drivers/otp` |
| Header | `pbl/drivers/otp.h` |
| Kconfig | `CONFIG_OTP`, enabled by the application |
| API | {apidoc}`API reference <drivers_otp>` |

## Drivers

| Driver | Compatible | Kconfig |
| --- | --- | --- |
| Flash security registers | — | `CONFIG_OTP_FLASH` |
| QEMU | — | `CONFIG_OTP_QEMU` |
| POSIX | — | `CONFIG_OTP_POSIX` |
