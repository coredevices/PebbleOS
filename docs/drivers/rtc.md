# RTC

An RTC counts UTC time in seconds on its own, through resets and, with a
backup supply, power loss. The kernel keeps the running wall clock, set from
the RTC at boot.

| | |
| --- | --- |
| Directory | `drivers/rtc` |
| Header | `pbl/drivers/rtc.h` |
| Kconfig | `CONFIG_RTC`, enabled by the application |
| Role | `RTC` |
| Sample | {doc}`../samples/drivers/rtc` |
| API | {apidoc}`API reference <drivers_rtc>` |

## Drivers

| Driver | Compatible | Kconfig |
| --- | --- | --- |
| Retained RAM | — | `CONFIG_RTC_RETAINED` |
| SiFli SF32LB | `sifli,sf32lb52-rtc` | `CONFIG_RTC_SF32LB` |
| QEMU RTC | — | `CONFIG_RTC_QEMU` |
| POSIX RTC | — | `CONFIG_RTC_POSIX` |
