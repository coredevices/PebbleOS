# Bluetooth

`subsys/bluetooth/` is the Bluetooth Low Energy stack interface. Its public
API is `include/pbl/bluetooth/`, enabled by `CONFIG_BT`; the
{apidoc}`API reference <bluetooth>` lists every module.

## Layering

`fw/comm/ble/` is the host-side BLE layer and carries substantial design
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
  ble["Host BLE layer<br/>fw/comm/ble/"]
  glue["subsys/bluetooth/"]
  nimble["NimBLE host<br/>third_party/nimble"]
  nrf["NimBLE controller<br/>nRF52 radio"]
  sifli["SiFli LCPU<br/>IPC, SF32LB52"]
  fake["Fake controller<br/>QEMU"]
  uart["H4 controller<br/>CONFIG_BT_HCI_UART"]
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

Beneath it sits NimBLE (`third_party/nimble`), glued in by
`subsys/bluetooth/`. The HCI transport is chosen per SoC (`BT_HCI`): the
NimBLE controller on the nRF52 radio, the SiFli LCPU over IPC on SF32LB52,
and a fake controller on QEMU that acknowledges every command so the host
runs without a radio. On QEMU the phone link is the emulator's serial
channel, `fw/comm/qemu/`.

## Modules

The {apidoc}`API reference <bluetooth>` documents every Bluetooth module.
