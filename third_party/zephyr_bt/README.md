# Zephyr Bluetooth host (BR/EDR)

Zephyr's Bluetooth host, run next to NimBLE as a second host for BR/EDR
(classic) Bluetooth. Both share the controller through the HCI router
(`subsys/bluetooth/hci_router.c`): NimBLE keeps LE, the Zephyr host gets the
BR/EDR traffic.

## Layout

- `zephyr/`: upstream Zephyr as a submodule, at
  [zephyrproject-rtos/zephyr@e35b686be50a](https://github.com/zephyrproject-rtos/zephyr/commit/e35b686be50a224e381fd35174be9fcb1a4ddd37),
  unmodified. Only its Bluetooth host (`subsys/bluetooth`), `lib/net_buf` and
  the headers they include are built; a sparse checkout of `include`,
  `lib/net_buf`, `subsys/bluetooth` and `subsys/logging` is enough.
- `config/`: the Zephyr Kconfig values for this host. `zephyr_bt_config.h` is
  generated from `pebble.conf` by evaluating Zephyr's own Bluetooth Kconfig, so
  defaults and dependencies are Zephyr's. To regenerate (only the submodule's
  `subsys/bluetooth` Kconfig files and `subsys/logging/Kconfig.template*` are
  read):

      python3 third_party/zephyr_bt/config/gen_config.py third_party/zephyr_bt/zephyr
- `port/`: the Zephyr kernel API, logging, device model and HCI driver the host
  needs, on the PebbleOS kernel. Its headers come before Zephyr's on the
  include path and replace `zephyr/kernel.h` and friends; Zephyr's generic
  headers (`sys/util.h`, lists, byte order, atomics, ...) are used as they are.
  - All Zephyr work queues (system, the host's RX queue) run on one thread,
    `ZephyrBT`. The host's "sending a synchronous command from the system work
    queue" path then covers every work item, so no TX processor thread is
    needed.
  - The HCI driver talks to the classic port of the HCI router. The router's
    receive thread copies packets for the host into host buffers and queues
    them to the `ZephyrRX` thread, which hands them to `bt_recv()`, as an
    HCI driver's RX thread would.
  - Zephyr asserts and panics end the host thread they happen on
    (`zbt_fatal()`) instead of crashing the watch; `zbt_stop()` then
    releases the rest.
- `ctl/`: starting and stopping the host (`zbt_start()`, `zbt_stop()`). It
  is in the kernel's own sections, while the host and `port/` have sections
  of their own in the kernel's RAM; workers keep the worker RAM.
- `include/zbt/zbt.h`: the API the rest of the firmware uses.
- `linker/`: Zephyr's iterable sections, and the host's RAM in the kernel's
  RAM. `zbt_start()` zeroes its `.bss` and copies the initial values of its
  `.data` in each time.
