Flash Memory Drivers
--------------------

Flash memory access is exposed through two separate APIs. The main API, defined
in `flash.h` and implemented in `flash_api.c`, is used almost everywhere. There
is also an alternate API used exclusively for performing core dumps, implemented
in `cd_flash_driver.c`. The alternate API is carefully implemented to be usable
regardless of how messed up the system state is in by not relying on any OS
services.

The flash APIs are written to be agnostic to the specific type of flash used.
They are written against the generic low-level driver interface defined in
`flash_impl.h`. Each low-level driver is specific to a combination of
flash memory part and microcontroller interface. The low-level drivers make no
use of OS services so that they can be used for both the main and core dump
driver APIs.
