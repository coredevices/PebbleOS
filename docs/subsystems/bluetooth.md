# Bluetooth

`subsys/bluetooth/` is the Bluetooth Low Energy stack interface. Its public
API is `include/pbl/bluetooth/`, enabled by `CONFIG_BT`; the
{apidoc}`API reference <bluetooth>` lists every module.

## Layering

`pebble/comm/ble/` is the host-side BLE layer and carries substantial design
prose:

- `gap_le_connect.c` — connection management and the "connection intent"
  abstraction that virtualizes the link across multiple clients.
- `gap_le_advert.c` — round-robin scheduling of multiple advertising jobs
  onto a controller that only holds one payload at a time (the comment
  predates the current controllers, but the scheduler it describes is still
  in use).
- `gap_le_connect_params.c` — a primer on connection intervals, slave
  latency and supervision timeouts, and why the firmware deviates from the
  spec-recommended parameter-update pause for iOS.

```{mermaid}
flowchart TD
  ble["Host BLE layer<br/>pebble/comm/ble/"]
  glue["pbl_bt API<br/>subsys/bluetooth/"]
  nimble["NimBLE host and port<br/>third_party/nimble"]
  nrf["NimBLE controller<br/>nRF52 radio"]
  sifli["SiFli LCPU<br/>IPC, SF32LB52"]
  fake["Fake controller<br/>QEMU"]
  uart["H4 controller<br/>CONFIG_NIMBLE_HCI_UART"]
  ble --> glue --> nimble
  nimble -- HCI --> nrf
  nimble -- HCI --> sifli
  nimble -- HCI --> fake
  nimble -- HCI --> uart

  classDef app fill:#fa4a36,stroke:#fa4a36,color:#ffffff
  classDef system fill:#fd8a7c,stroke:#fd8a7c,color:#171717
  classDef base fill:#525252,stroke:#525252,color:#ffffff
  class ble app
  class glue,nimble system
  class nrf,sifli,fake,uart base
```

Beneath it sits NimBLE (`third_party/nimble`), which `subsys/bluetooth/`
implements the `pbl_bt` API on. NimBLE can also be used on its own, with
`CONFIG_NIMBLE`: its port runs the host, and the link layer when the
controller is in the image, on threads that `nimble_port_pebble_init()`
starts, and the {doc}`NimBLE samples <../samples/index>` use its API
directly. The HCI transport is chosen per SoC (`NIMBLE_HCI`): the
NimBLE controller on the nRF52 radio, the SiFli LCPU over IPC on SF32LB52,
and a fake controller on QEMU that acknowledges every command so the host
runs without a radio. On QEMU the phone link is the emulator's serial
channel, `pebble/comm/qemu/`.

## NimBLE port

`third_party/nimble` builds NimBLE from its submodule, with every option of
its `syscfg.yml` files generated into Kconfig (`scripts/README.md`). The
PebbleOS side of it is split by what it does:

| Directory | Contents |
| --- | --- |
| `port/` | The NPL on the kernel, `nimble_port_pebble_init()` and the threads it starts, the nRF52 interrupts and the TRNG |
| `transport/` | The HCI transports, chosen with `NIMBLE_HCI`, and the identity address the controller is given |
| `store/` | Saving NimBLE's config store in a settings file |

An application brings NimBLE up as on its other ports: `nimble_port_init()`,
then `ble_hs_cfg`, the store with `ble_store_config_init()` and the GATT
services, then `nimble_port_pebble_init()`. With
`NIMBLE_BLE_STORE_CONFIG_PERSIST`, on by default with `CONFIG_SETTINGS`, the
store keeps the keys of bonded peers and their CCCDs in `/nimble_store`.

The Bluetooth subsystem uses that store too. The firmware hands it the
bondings of its own database at start, which moves those of older firmware
into it, and the recovery firmware those of shared PRF storage, as it has no
filesystem.

## Modules

The {apidoc}`API reference <bluetooth>` documents every Bluetooth module.
