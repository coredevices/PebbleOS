# Bluetooth

`subsys/bluetooth/` is the Bluetooth Low Energy stack interface. Its public
API is `include/pbl/bluetooth/`, enabled by `CONFIG_BT`; the
{apidoc}`API reference <bluetooth>` lists every module.

## Layering

The subsystem does not wrap NimBLE: it runs its host and adds what an
application needs around it, and everything else is done with NimBLE directly.
A connection is named by its NimBLE handle everywhere, and whoever needs its
events follows them with `ble_gap_event_listener_register()`.

```{mermaid}
flowchart TD
  fw["Firmware<br/>pebble/services/bluetooth/"]
  glue["Bluetooth subsystem<br/>subsys/bluetooth/"]
  nimble["NimBLE host and port<br/>third_party/nimble"]
  nrf["NimBLE controller<br/>nRF52 radio"]
  sdc["SoftDevice Controller<br/>nRF54L, nonfree"]
  sifli["SiFli LCPU<br/>IPC, SF32LB52"]
  fake["Fake controller<br/>QEMU"]
  uart["H4 controller<br/>CONFIG_NIMBLE_HCI_UART"]
  fw --> glue --> nimble
  fw --> nimble
  nimble -- HCI --> nrf
  nimble -- HCI --> sdc
  nimble -- HCI --> sifli
  nimble -- HCI --> fake
  nimble -- HCI --> uart

  classDef app fill:#fa4a36,stroke:#fa4a36,color:#ffffff
  classDef system fill:#fd8a7c,stroke:#fd8a7c,color:#171717
  classDef base fill:#525252,stroke:#525252,color:#ffffff
  class fw app
  class glue,nimble system
  class nrf,sdc,sifli,fake,uart base
```

What the subsystem has:

| Part | Header | What for |
| --- | --- | --- |
| Start and stop | `pbl/bluetooth/init.h` | The host task, the services and the identity address |
| Bonds | `pbl/bluetooth/bond.h` | The store, and what is in it |
| Pairing | `pbl/bluetooth/gap.h`, `pairing_confirm.h`, `pairability.h` | Who may pair, and the confirmation by the user |
| Response time | `pbl/bluetooth/responsiveness.h` | The connection parameters, see below |
| Advertising | `pbl/bluetooth/adv.h` | What is on air, and at which interval, see below |
| GATT services | `pbl/bluetooth/gatts.h`, `subsys/bluetooth/services/` | What the device serves, see below |
| GATT clients | `pbl/bluetooth/clients/` | What it uses of a peer, see below |

The first four are the subsystem proper, `CONFIG_BT`. Advertising, the GATT
server helpers and the clients each have an option of their own and build
with NimBLE alone. Changes to the bonds and pairings are reported to listeners
(`PBL_BT_BOND_LISTENER_DEFINE()`, `PBL_BT_PAIRING_LISTENER_DEFINE()`). With
`CONFIG_SHELL` the subsystem has the `bt` command set, to which its users add
their commands.

Beneath it sits NimBLE (`third_party/nimble`), which can also be used on its
own, with
`CONFIG_NIMBLE`: its port runs the host, and the link layer when the
controller is in the image, on threads that `nimble_port_pebble_init()`
starts, and the {doc}`NimBLE samples <../samples/index>` use its API
directly. The HCI transport is chosen per SoC (`NIMBLE_HCI`): the
NimBLE controller on the nRF52 radio, the Nordic SoftDevice Controller on
the nRF54L, from the nonfree libraries and called directly with no HCI
framing, the SiFli LCPU over IPC on SF32LB52, and a fake controller on QEMU that acknowledges every command so the host
runs without a radio. On QEMU the phone link is the emulator's serial
channel, `pebble/debug/qemu/`.

## NimBLE port

`third_party/nimble` builds NimBLE from its submodule, with every option of
its `syscfg.yml` files generated into Kconfig (`scripts/README.md`). The
PebbleOS side of it is split by what it does:

| Directory | Contents |
| --- | --- |
| `port/` | The NPL on the kernel, `nimble_port_pebble_init()` and the threads it starts, the nRF52 interrupts and the TRNG |
| `transport/` | The HCI transports, chosen with `NIMBLE_HCI`, and the identity address the controller is given |
| `store/` | The backends NimBLE's config store persists in |

An application brings NimBLE up as on its other ports: `nimble_port_init()`,
then `ble_hs_cfg`, the store with `ble_store_config_init()` and the GATT
services, then `nimble_port_pebble_init()`. The store keeps the keys of bonded
peers and their CCCDs where its backend, `NIMBLE_STORE_BACKEND`, says:

| Backend | Keeps |
| --- | --- |
| `NIMBLE_STORE_BACKEND_RAM` | Nothing across a reset |
| `NIMBLE_STORE_BACKEND_SETTINGS` | Everything in `/nimble_store`, along with a local IRK generated once. The default with `CONFIG_SETTINGS` |
| `NIMBLE_STORE_BACKEND_PRF` | One bonding in the PRF store; the local IRK is left to whoever uses the store |

With `NIMBLE_STORE_SETTINGS_PRF_MIRROR` the settings backend shares its
bonding with the PRF store as well: the one written last goes there, and the
one found there is taken when the store loads.

The PRF store (`pbl/bluetooth/prf_store.h`, `BT_PRF_STORE`, in
`subsys/bluetooth/prf_store`) is a flash partition of its own with the one
bonding and the root keys the recovery firmware and the normal firmware
share. It needs NimBLE for its types only. Recovery firmware in the field
reads it, so the layout a device ships with is fixed. `BT_PRF_STORE_LAYOUT`
chooses it:

| Layout | Is |
| --- | --- |
| `BT_PRF_STORE_LAYOUT_RECORDS` | A log of 140-byte records with one CRC, each holding all the store keeps: the last valid one is the store. The default |
| `BT_PRF_STORE_LAYOUT_LEGACY` | Entries of 256 bytes made of fields with a CRC each. What Asterix, Obelix and Getafix shipped with, and keep |

The types that describe a layout are the store's own: a bonding goes in and
comes out as the keys NimBLE stores for a peer, `struct ble_store_value_sec`.
What writes returns an errno if the flash failed.

The Bluetooth subsystem uses that store too, and it is the only place bonds
are kept. `pbl/bluetooth/bond.h` lists them by the peer's identity address,
deletes them, and reports the ones the stack adds, changes and deletes.

The firmware builds on that in its Bluetooth service:

- It keeps one bond, the phone's: a new one replaces the others.
- The normal firmware keeps the store in the settings file, mirrored to the PRF
  store for the recovery firmware. That is also how a bond made in recovery
  firmware, or by firmware that kept bonds in a database of its own, comes in.
- The recovery firmware has no filesystem and uses the PRF backend.
- Both take the local IRK from the root keys in the PRF store, so that a phone
  sees one watch. The phone's name is kept there as well.

## Response time

A connection is slow and frugal, or fast and costly, and which one is a matter
of its parameters. Whoever needs it fast asks for a response time on a
connection with `pbl_bt_responsiveness_request()`, with a request it owns
(`struct pbl_bt_responsiveness_req`) and for a limited period; the fastest one asked for is negotiated with the central:

| Response time | Interval | Peripheral latency | Supervision timeout |
| --- | --- | --- | --- |
| `PBL_BT_RESPONSE_TIME_MAX` | 45 to 60 ms | 4 | 6 s |
| `PBL_BT_RESPONSE_TIME_MIN` | 15 ms | 0 | 6 s |

These are the defaults of the `BT_RESPONSE_TIME_*` options.

The interval is the time between two connection events, where both ends meet
on a channel; the latency is the number of them the peripheral may skip when
it has nothing to send, which saves it power without costing it latency of its
own; the supervision timeout is how long the link survives without one.

Nothing is asked of the central before the first request, nor within a second
of connecting: the specification recommends five, but iOS connects with a
720 ms supervision timeout and never raises it on its own, so the link is
fragile until then. A request the central does not answer in 5 s is made
again, three times at most; one that collides with a procedure of the central
is made again 2 s later and does not count. A response time taken back holds
for 2 s more, so that consecutive transfers do not go back and forth.

## The link of the firmware

The watch is a peripheral with one connection, the link
(`pebble/services/bluetooth/link.c`). It follows it with a GAP listener, and
tells the rest of the firmware about it:

- Any connection and disconnection, with `PEBBLE_BT_CONNECTION_EVENT`.
- The gateway: the link becomes the gateway's once it is encrypted and its
  peer is bonded, and only then are the services of the phone used. That, and
  the loss of it, is told to the gateway service
  (`pebble/services/bluetooth/gateway.c`), which runs the clients on the main thread.
- The device name of the peer, read with the GAP client and kept with its bond.

## GATT clients

The clients of the services of a peer are built on NimBLE alone, and can be
used without the rest of the subsystem:

- `pbl/bluetooth/clients/gatt.h` (`BT_GATT_CLIENT`) is what clients share.
  `pbl_bt_gattc_discover()` finds one service and the value and CCCD handles
  of its characteristics. A service client (`PBL_BT_GATTC_SERVICE_DEFINE()`)
  builds on it for one service of one connection: it subscribes, writes and
  reads by characteristic, and reports the outcomes and the notifications.
  Discoveries, subscriptions, reads and writes with a response are queued and
  run one at a time on the host task, since NimBLE cannot tell the responses
  of concurrent procedures apart; a write without a response is sent at once.
  Notification listeners are fed from a GAP event listener of the clients'
  own, whoever handles the GAP events of a connection.
- `pbl/bluetooth/clients/gap.h` reads the device name of a peer.
- `pbl/bluetooth/clients/service_changed.h` tells when the services of a peer
  change, for the other clients to look for theirs again.
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

The firmware uses them for everything it needs from the phone, which it calls
the gateway: the bonded peer, once its link is encrypted.
`pebble/services/bluetooth/gateway.c` starts a client of each service when the
gateway connects, and stops them when it goes:

| Service | Client | What for |
| --- | --- | --- |
| PPoGATT | `subsys/pp/transports/gatt/` | Sessions with the phone app, and with other apps: each hosts an instance of the service |
| Generic Attribute | The Service Changed client | When the phone indicates it, every client looks for its service again |
| ANCS | `pebble/services/notifications/ancs/ancs.c`, on the ANCS client | iOS notifications |
| AMS | `pebble/services/music/ams.c`, on the AMS client | iOS media control |

Each looks for its own service, in that order, so that the session with the
phone app comes first. What a client reports on the NimBLE host thread is
passed on to KernelMain, where the sessions (queueing, timeouts, the liveness
check, the music service, the PPoGATT state machine) run.

## GATT services

`pbl/bluetooth/gatts.h` (`BT_GATT_SERVER`) is what the services of the device
share: adding their definitions, notifying a value, and the order they are
added in. Each service is defined with `PBL_BT_GATTS_SERVICE_DEFINE()` and a
position, and `pbl_bt_start()` adds them by it. The order sets the attribute
handles, which bonded peers cache, so it does not change for services that
shipped:

| Position | Service | Where |
| --- | --- | --- |
| 10 | Device Information | `subsys/bluetooth/services/dis` |
| 20 | Pebble Pairing | `subsys/bluetooth/services/pps` |
| 30 | Battery | `subsys/bluetooth/services/bas` |
| 40 | Reversed PPoGATT | `subsys/pp/transports/gatt`, with the transport that uses it |

## Advertising

`pbl/bluetooth/adv.h` (`BT_ADV`) keeps connectable advertising in line with
what there is to advertise. Whoever advertises describes its advertisements,
each a function that sets the payloads, in order of priority, and enables them
as they apply. The first one enabled is on air while there is no connection:
at a short interval for a while, then at a long one. Advertising resumes when
a connection ends or fails to establish, is tried again a second later when
the stack refuses it, and is aired anew when the host synchronizes with its
controller again. A peer that connects and drops over and over stops bringing
the short interval back.

The firmware has two (`pebble/services/bluetooth/advertising.c`): one to be
discovered, while there is no bond or a pairing screen is open, and one for
the bonded phone to reconnect to, with an empty payload. They air at 20 ms,
then at 1022.5 ms. The second one is behind
`SERVICE_BLUETOOTH_ADV_RECONNECT`, which the recovery firmware leaves off: it
only advertises to be discovered. A new phone, or the bonded one after it
deleted its side of the bond, can then always find it.

## Modules

The {apidoc}`API reference <bluetooth>` documents every Bluetooth module.
