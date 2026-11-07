# Transports

A transport carries sessions over a link. It implements
`struct pbl_pp_transport` (`include/pbl/pp/transport.h`) and lives in
`subsys/pp/transports/`:

| Transport | Kconfig | Link | Driven by | Priority | Shared |
|---|---|---|---|---|---|
| GATT (PPoGATT) | `PP_TRANSPORT_GATT` | The phone's PPoGATT service over BLE | The kernel LE client | 2 | no |
| Console | `PP_TRANSPORT_CONSOLE` | Frames on the serial console | The console | 1 | yes |
| QEMU | `PP_TRANSPORT_QEMU` | The emulator's serial link | The QEMU serial protocol | 0 | no |

Each is enabled with the link it uses. The code that drives the link calls
the transport through its header in `include/pbl/pp/transports/`.

## Contract

```{mermaid}
sequenceDiagram
  participant D as Link driver
  participant T as Transport
  participant S as Session
  D->>T: link up
  T->>S: pbl_pp_session_open(transport, destination, app UUID)
  opt peer known up front
    T->>S: pbl_pp_session_set_capabilities()
  end
  loop while the link is up
    D->>T: received bytes
    T->>S: pbl_pp_session_receive()
    S-->>T: send_next(session), on KernelMain
    T->>S: pbl_pp_session_send_queue_length() / _copy() / _peek()
    T->>D: send
    D->>T: delivered
    T->>S: pbl_pp_session_send_queue_consume()
  end
  D->>T: link down
  T->>S: pbl_pp_session_close()
```

A transport:

- Opens a session once its link can carry messages, with the destination
  and, for app sessions, the UUID of the watch app. Opening a system session
  may close another transport's system session through its `close` callback.
- Feeds every received byte to `pbl_pp_session_receive()`, from one context
  at a time.
- Drains the send queue when `send_next` runs. It may leave data queued, for
  instance while waiting for acknowledgments, and calls
  `pbl_pp_session_send_next()` to be called again.
- Consumes only what the peer has received: the queue holds the bytes until
  then, so they can be sent again.
- Closes the session when the link goes down; queued data is dropped.

| Field | |
|---|---|
| `name` | For logs |
| `priority` | Which system session `pbl_pp_session_get_system()` prefers |
| `shared` | Whether its system sessions coexist with others |
| `send_next` | Mandatory |
| `close` | Close a session for a new system session; NULL if the watch cannot close it, which then refuses new system sessions |
| `reset` | Reset a session on request, see `pbl_pp_session_reset()`; NULL if not supported |
| `set_responsiveness` | Apply a connection response time; NULL grants every request at once |

Callbacks get the session, not the transport's context, and may run after
the transport has closed it: the transport looks the session up among its
own, under its own lock, and ignores calls for sessions it no longer owns.
See the locking rules in {doc}`index`.

### Adding a transport

A new transport, for instance over TCP/IP, implements the callbacks above
and calls the session API; nothing else changes. Who may open a session is
the only access control: a system session reaches every private endpoint,
including firmware updates and the factory registry. A transport whose link
anyone can reach, such as a network socket, must authenticate the Pebble
mobile app before it opens a system session.

## GATT (PPoGATT)

PPoGATT carries the Pebble protocol over BLE GATT, with ordering,
acknowledgments and retransmission on top of unacknowledged GATT writes and
notifications. Each PPoGATT service is one session: the Pebble mobile app
hosts one, and third party apps may host their own.

It has two roles:

| Role | GATT server | Data from watch to phone | Data from phone to watch |
|---|---|---|---|
| Forward | The phone | Write without response | Notification |
| Reversed | The watch | Notification | Write without response |

In the forward role the watch is a client of the phone's service, found
during service discovery. In the reversed role the watch hosts the service
(`subsys/bluetooth`) and the phone is its client. Once the phone starts a
reversed handshake on a connection, it takes over from forward PPoGATT on that
connection.

### Service

| Characteristic | |
|---|---|
| Meta | Read: the server's versions, its app's UUID and the session type |
| Data | PPoGATT packets in both directions |

The meta characteristic holds:

| Offset | Size | Field | Since |
|---|---|---|---|
| 0 | 1 | Minimum PPoGATT version | v0 |
| 1 | 1 | Maximum PPoGATT version | v0 |
| 2 | 16 | App UUID: the system UUID for the Pebble mobile app | v0 |
| 18 | 1 | Session type: 0 from the UUID, 1 hybrid | v1 |

The watch uses the highest version both support (0 or 1), and ignores
servers whose minimum version is above 1 or whose UUID is invalid. The session is hybrid if the session
type says so, a system session for the system UUID, and an app session
otherwise. Recovery firmware only connects to the Pebble mobile app.

### Packets

Each GATT write or notification is one packet, a 1-byte header followed by
the payload:

| Bits | Field |
|---|---|
| 0-2 | Type |
| 3-7 | Sequence number, modulo 32 |

| Type | Value | Payload |
|---|---|---|
| Data | 0 | Pebble protocol bytes, up to the ATT MTU minus 4 |
| Ack | 1 | None; acknowledges data up to and including the sequence number |
| Reset request | 2 | From the watch: its PPoGATT version and serial number |
| Reset complete | 3 | Version 1: the sender's receive and transmit windows |

The Pebble protocol stream is cut into data packets regardless of message
boundaries.

### Handshake

```{mermaid}
sequenceDiagram
  participant W as Watch (client)
  participant P as Phone (PPoGATT server)
  Note over W,P: forward role, after service discovery
  W->>P: read Meta
  P-->>W: versions, app UUID, session type
  W->>P: subscribe to Data
  W->>P: Reset request (version, serial)
  P->>W: Reset complete (windows, v1)
  Note over W: open the session
  W->>P: Reset complete (windows, v1)
  Note over W,P: data flows both ways
```

In the reversed role the phone subscribes to the watch's Data
characteristic and sends the first reset request.

A reset, started by either side, starts the session over: the watch closes
its session, dropping what was queued, and opens a new one once the reset
completes. Either side may ask for one when it loses track of the sequence
numbers.

### Data and acknowledgments

Each side sends up to a window of unacknowledged data packets. Version 0 has
a window of 4. In version 1 each side announces the windows it wants in its
reset complete, and each side uses the smaller of the two: the watch asks to
send up to 31 packets, and to receive about 4500 bytes' worth.

```{mermaid}
sequenceDiagram
  participant W as Watch
  participant P as Phone
  W->>P: Data sn=0
  W->>P: Data sn=1
  W->>P: Data sn=2
  P->>W: Ack sn=2
  Note over W: consume sn 0-2 from the send queue
  P->>W: Data sn=0
  P->>W: Data sn=1
  Note over W: v1: coalesce acks, up to 200 ms or half the window
  W->>P: Ack sn=1
```

Receiving:

- A data packet with the expected sequence number goes to the session and is
  acknowledged; any other is dropped, and the phone sends it again.
- Version 0 acknowledges each packet right away. Version 1 coalesces
  acknowledgments, sending one when half the receive window is pending or
  after 200 ms.

Sending:

- An acknowledgment releases every packet up to its sequence number, and the
  watch consumes their bytes from the session's send queue.
- A repeated acknowledgment of the last packet means the phone missed what
  followed; the watch lets the timeout resend it rather than resending at
  once.
- An acknowledgment out of range resets the session.

### Recovery

```{mermaid}
stateDiagram-v2
  direction LR
  Open --> Open: ack timeout,<br/>resend from the oldest<br/>unacknowledged packet
  Open --> Resetting: 2 timeouts in a row,<br/>or a bad ack
  Resetting --> Open: reset complete
  Resetting --> Resetting: no reset complete in time
  Resetting --> Disconnect: 5 resets in a row
  Disconnect --> Stalled: 2 disconnects in a row
  Stalled --> Open: the phone resets
```

- An acknowledgment is expected within 5 to 6 s. When it does not come,
  the watch sends again from the oldest unacknowledged packet.
- After 2 timeouts in a row, or a reset complete that does not come, the
  watch resets the session.
- After 5 resets in a row it disconnects the phone. After 2 such disconnects
  in a row it stops resetting and waits for the phone to reset, so a broken
  phone app cannot make the watch reconnect forever.
- When the BLE stack has no buffers left, the watch tries the write again
  20 ms later, doubling the delay each time up to 200 ms.

## Console

The console transport carries the Pebble protocol in frames on the serial
console's channel `P`, between text lines (see {doc}`../console`). It lets
host tools talk to a watch over its debug port, alongside the phone: its
sessions are hybrid and shared, and the phone's system session stays the
preferred one.

The first byte of a frame's payload is an opcode:

| Opcode | Value | Rest of the payload |
|---|---|---|
| Data | 1 | Pebble protocol bytes |
| Open | 2 | None |
| Close | 3 | None |

```{mermaid}
sequenceDiagram
  participant H as Host
  participant W as Watch
  H->>W: Open
  Note over W: open a hybrid session
  W->>H: Open
  H->>W: Data (Pebble protocol)
  W->>H: Data seq=n
  H->>W: ack seq=n
  Note over W: consume the frame's bytes,<br/>send the next one
  H->>W: Close
  W->>H: Close
```

Frames from the watch carry a sequence number and are sent one at a time:
the watch sends a frame again every 200 ms until the host acknowledges it,
and closes the session after 25 tries. A data frame for a session the watch
does not have, after a restart for instance, gets a close back.

## QEMU

The QEMU transport carries the Pebble protocol over the emulator's serial
link, in the QEMU serial protocol's frames:

| Offset | Size | Field |
|---|---|---|
| 0 | 2 | Signature, `0xFEED` |
| 2 | 2 | Protocol: 1 for the Pebble protocol |
| 4 | 2 | Payload length, up to 2048 |
| 6 | length | Payload |
| 6 + length | 2 | Signature, `0xBEEF` |

All fields are big endian. The link is reliable, so the transport sends the
whole send queue at once and consumes it.

The transport emulates a phone connection. It opens a hybrid session when
Bluetooth starts, if the emulator is set to start connected, and closes it
when Bluetooth stops. The emulator's Bluetooth connection messages only tell
apps that the phone connected or disconnected; the session stays open. Once
the first app has started, the transport logs `Ready for communication.`,
which host tools wait for before installing.
