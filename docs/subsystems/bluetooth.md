# Bluetooth

`subsys/bluetooth/` is the Bluetooth Low Energy stack interface. Its public
API is `include/pbl/bluetooth/`, enabled by `CONFIG_BT`; the
{apidoc}`API reference <bluetooth>` lists every module.

## Layering

`pebble/comm/ble/` is the host-side BLE layer and carries substantial design
prose:

- `gap_le_connect.c` — connection management: the bonded phone, once its
  link is encrypted, is the gateway whose services the firmware uses.
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
| `store/` | The backends NimBLE's config store persists in, and the PRF store |

An application brings NimBLE up as on its other ports: `nimble_port_init()`,
then `ble_hs_cfg`, the store with `ble_store_config_init()` and the GATT
services, then `nimble_port_pebble_init()`. The store keeps the keys of bonded
peers and their CCCDs where its backend, `NIMBLE_STORE_BACKEND`, says:

| Backend | Keeps |
| --- | --- |
| `NIMBLE_STORE_BACKEND_RAM` | Nothing across a reset |
| `NIMBLE_STORE_BACKEND_SETTINGS` | Everything in `/nimble_store`, along with a local IRK generated once. The default with `CONFIG_SETTINGS` |
| `NIMBLE_STORE_BACKEND_PRF` | One bonding in the PRF store; its local IRK is the identity root key kept there |

The PRF store (`pbl/bluetooth/prf_store.h`, `NIMBLE_STORE_PRF`) is a flash
partition of its own with the one bonding and the root keys the recovery
firmware and the normal firmware share. Recovery firmware in the field reads
it, so its layout is fixed.

The Bluetooth subsystem uses that store too, and it is the only place bonds
are kept. `pbl/bluetooth/bond.h` lists them by the peer's identity address,
deletes them, and reports the ones the stack adds, changes and deletes.

The firmware builds on that in its Bluetooth service:

- It keeps one bond, the phone's: a new one replaces the others.
- The normal firmware keeps the store in the settings file and copies its bond
  to the PRF store, for the recovery firmware. At start it takes the pairing
  of the PRF store instead, which is how a bond made in recovery firmware, or
  by firmware that kept bonds in a database of its own, comes in.
- The recovery firmware has no filesystem and uses the PRF backend.
- Both take the local IRK from the root keys in the PRF store, so that a phone
  sees one watch. The phone's name is kept there as well.

## GATT clients

The clients of the services of a peer are built on NimBLE alone, and can be
used without the rest of the subsystem:

- `pbl/bluetooth/clients/gatt.h` (`BT_GATT_CLIENT`) is what clients share.
  `pbl_bt_gattc_discover()` finds one service and the value and CCCD handles
  of its characteristics, one discovery at a time, since NimBLE cannot tell
  the responses of concurrent ones apart. A service client
  (`PBL_BT_GATTC_SERVICE_DEFINE()`) builds on it for one service of one
  connection: it subscribes, writes and reads by characteristic, and reports
  the outcomes and the notifications. Notification listeners are fed by
  whoever handles the GAP events of a connection, with
  `pbl_bt_gattc_notify_rx()`.
- `pbl/bluetooth/clients/ancs.h` (`BT_ANCS_CLIENT`) is the client of the Apple
  Notification Center Service. It reports the notifications of the device,
  and on request gets their attributes and those of their apps, gathering
  the responses that come in parts, and performs their actions. A client
  (`PBL_BT_ANCS_CLIENT_DEFINE()`) serves one connection and runs one request
  at a time; the values of the service are in `pbl/bluetooth/ancs.h`. The
  {doc}`iOS clients sample <../samples/subsys/bluetooth/ios_clients>` shows
  it.
- `pbl/bluetooth/clients/ams.h` (`BT_AMS_CLIENT`) is the client of the Apple
  Media Service: it sends commands to the media player, registers for
  attributes of the player, the queue and the track and reports their values
  as the device updates them, and reads the whole of a value that was cut. A
  client (`PBL_BT_AMS_CLIENT_DEFINE()`) serves one connection; the values of
  the service are in `pbl/bluetooth/ams.h`. The same sample shows it.

The firmware's notification and media sessions (queueing, timeouts, the
liveness check, the music service) sit on top of those two in
`pebble/comm/ble/kernel_le_client/`, on KernelMain: what a client reports on
the NimBLE host thread is passed on there.

PPoGATT, the other client of the phone's services, still goes through the
discovery and subscription layer of `pebble/comm/ble/`.

## Advertising

The firmware advertises with NimBLE directly, from its Bluetooth service
(`pebble/services/bluetooth/advertising.c`): to be discovered while there is
no bond or a pairing screen is open, and for the bonded phone to reconnect
otherwise, with an empty payload. It starts at a 20 ms interval and falls back
to 1022.5 ms, and is silent while connected.

## Modules

The {apidoc}`API reference <bluetooth>` documents every Bluetooth module.
