# Pebble protocol

The Pebble protocol carries messages between the watch and phone apps: the
Pebble mobile app (notifications, firmware updates, app installs, settings
sync, ...) and third party phone apps talking to their watch apps. Every
message is addressed to an *endpoint*, a 16-bit ID naming the service that
handles it, such as Put Bytes or BlobDB.

`subsys/pp/` implements it. Its public API is in `include/pbl/pp/`:

| Header | For |
|---|---|
| `protocol.h` | Wire definitions: message header, endpoint IDs, capabilities |
| `session.h` | Finding sessions, their capabilities, session listeners |
| `send.h` | Queueing outbound messages |
| `endpoint.h` | Handling inbound messages |
| `transport.h` | Links that carry sessions |
| `transports/*.h` | Entry points of each transport for the code that drives its link |

```{toctree}
:maxdepth: 1

protocol.md
transports.md
```

{doc}`protocol` describes the messages on the wire, and {doc}`transports`
the links that carry them.

## Layers

The subsystem has three layers, each knowing only the one below:

```{mermaid}
flowchart TB
  subgraph users["Endpoints, senders, listeners"]
    direction LR
    pb["Put Bytes"]
    blob["BlobDB"]
    music["Music"]
    more["..."]
  end
  subgraph core["Sessions (subsys/pp)"]
    direction LR
    rx["Receive<br/>header parsing,<br/>endpoint lookup"]
    tx["Send queue<br/>send buffers, jobs"]
    sess["Session registry<br/>lookup, capabilities,<br/>listeners"]
  end
  subgraph transports["Transports (subsys/pp/transports)"]
    direction LR
    gatt["GATT<br/>(PPoGATT)"]
    console["Console"]
    qemu["QEMU"]
  end
  users --> core
  core --> transports
  gatt --- ble["Kernel LE client"]
  console --- cons["Console frames"]
  qemu --- qser["QEMU serial"]

  classDef app fill:#fa4a36,stroke:#fa4a36,color:#ffffff
  classDef system fill:#fd8a7c,stroke:#fd8a7c,color:#171717
  classDef base fill:#525252,stroke:#525252,color:#ffffff
  class pb,blob,music,more app
  class rx,tx,sess system
  class gatt,console,qemu,ble,cons,qser base
```

- Endpoints and senders use sessions and know nothing about transports.
- Sessions parse, dispatch and queue messages, and know nothing about the
  links.
- Transports move bytes over a link and open and close sessions. The code
  that drives each link (the kernel LE client, the console, the QEMU serial
  protocol) calls its transport through the transport's header.

## Sessions

A session is a Pebble protocol connection with one peer over one transport.
It has a destination:

| Destination | Peer | Session type |
|---|---|---|
| System | The Pebble mobile app | `PBL_PP_SESSION_SYSTEM` |
| App | A third party phone app, bound to the UUID of its watch app | `PBL_PP_SESSION_APP` |
| Hybrid | Both, as the Pebble Android app relays PebbleKit traffic | `PBL_PP_SESSION_SYSTEM` |

`pbl_pp_session_get_system()` returns the system session. When more than one
is open, for example the console next to PPoGATT during development, the one
on the transport with the highest priority wins. `pbl_pp_session_get_app()`
returns the session serving a watch app: an app session bound to its UUID,
or else a hybrid session or an app session whose app is unknown. The
`app_session` service applies this to the running app, and gives PebbleKit
JS apps the system session.

Opening a system or hybrid session closes the existing system session through
its transport, unless either is on a *shared* transport like the console.
This keeps one Pebble mobile app connection at a time: when the app comes
back over another link, or relaunches after a crash, the last session to open
wins.

```{mermaid}
stateDiagram-v2
  direction LR
  [*] --> Open: pbl_pp_session_open()
  Open --> Open: messages, capabilities
  Open --> Closed: pbl_pp_session_close()<br/>by its transport
  Open --> Closed: a new system session<br/>closes it
  Closed --> [*]
```

When a session opens, it asks the phone for its version and capabilities
(see {ref}`pp-version`). Code checks them with
`pbl_pp_session_has_capability()` before using a feature the phone may not
support.

Sessions close at any time. A session pointer is a handle that every call
validates, so code may keep one around and find out later that it is gone;
calls on a closed session fail, or do nothing.

### Listeners

Code that reacts to sessions defines a listener next to its own code. The
subsystem collects listeners at link time and calls each for every event:

| Event | When |
|---|---|
| `PBL_PP_SESSION_OPENED` | The session opened |
| `PBL_PP_SESSION_CLOSED` | The session is closing; it is still valid during the call |
| `PBL_PP_SESSION_CAPABILITIES` | The phone announced the session's capabilities |

```c
static void prv_session_cb(struct pbl_pp_session *session, enum pbl_pp_session_event event,
                           void *user_data) {
  if (event == PBL_PP_SESSION_CLOSED && pbl_pp_session_is_system(session)) {
    pbl_sys_work_call(prv_handle_disconnect, NULL);
  }
}

PBL_PP_SESSION_LISTENER_DEFINE(my_service, prv_session_cb, NULL);
```

Data logging, app launch, analytics and the cache of the system session's
capabilities are listeners. Listeners run in the context of the transport,
which may hold its own lock, so they must not block. Apps learn about
sessions through `PEBBLE_COMM_SESSION_EVENT`, which the subsystem puts for
every session that opens or closes.

## Endpoints

Each endpoint is defined next to the code that handles it, and collected at
link time. The ID comes from `protocol.h`, where all of them are listed:

```c
static void prv_ping_handler(struct pbl_pp_session *session, const uint8_t *data,
                             size_t length) {
  ...
}

PBL_PP_ENDPOINT_DEFINE(PBL_PP_ENDPOINT_PING, PBL_PP_ACCESS_ANY, prv_ping_handler);
```

| Macro | Handling |
|---|---|
| `PBL_PP_ENDPOINT_DEFINE()` | The default receiver copies the payload to the kernel heap and runs the handler on SysWorkq |
| `PBL_PP_ENDPOINT_DEFINE_MAIN()` | The same, on KernelMain, for handlers whose events must stay ordered with `PEBBLE_COMM_SESSION_EVENT` |
| `PBL_PP_ENDPOINT_DEFINE_RECEIVER()` | A receiver of the endpoint's own buffers the message, such as Put Bytes into its download buffer or AppMessage into the app's inbox |

The access level decides who may use the endpoint: `PBL_PP_ACCESS_PRIVATE`
endpoints only accept messages on system sessions, `PBL_PP_ACCESS_ANY` ones
on app sessions too. Messages for unknown or disallowed endpoints are
answered on the meta endpoint (see {ref}`pp-meta`).

An endpoint exists if the code defining it is built. Endpoints left out of
recovery firmware are guarded with `CONFIG_RECOVERY_FW`, which keeps PRF to
the few endpoints it needs to update the firmware. Each definition is a
symbol named after its ID, so defining an ID twice fails to link, and the
handler's type is checked.

### Receiving

```{mermaid}
sequenceDiagram
  participant L as Link
  participant T as Transport
  participant S as Session
  participant R as Receiver
  participant H as Handler (SysWorkq or KernelMain)
  L->>T: bytes
  T->>S: pbl_pp_session_receive()
  Note over S: collect the 4-byte header,<br/>look the endpoint up,<br/>check its access
  alt unknown or disallowed
    S-->>S: meta error, skip the payload
  else accepted
    S->>R: prepare(length)
    loop payload bytes, split anywhere
      T->>S: pbl_pp_session_receive()
      S->>R: write()
    end
    S->>R: finish()
    R->>H: queue the message
    H->>H: handler(session, payload, length)
  end
```

The transport may split the bytes anywhere: a header may arrive in pieces and
a write may hold the end of one message and the start of the next. Each
session has one message in progress at a time. The default receiver drains
one queue per task, one message per callback, so a slow handler cannot starve
the task watchdog and bursts cannot overflow the task queue.

## Sending

| API | Use |
|---|---|
| `pbl_pp_send()` | A complete message, copied |
| `pbl_pp_send_buffer_begin()`, `_write()`, `_end()` | A message built piecemeal |
| `pbl_pp_send_buffer_begin_bulk()` | The same, for bulk transfers |
| `pbl_pp_send_job_submit()` | A message whose memory the caller owns, queued without a copy |

Send buffers come from two budgets of about 1 KiB each, normal and bulk, so
that bulk transfers such as data logging or screenshots cannot starve
interactive messages; a shared reserve lets either go a little further.
Beginning a buffer blocks until there is room or the timeout expires.

```{mermaid}
sequenceDiagram
  participant C as Caller
  participant S as Session
  participant M as KernelMain
  participant T as Transport
  participant L as Link
  C->>S: pbl_pp_send_buffer_begin()
  Note over S: wait for room in the budget
  C->>S: pbl_pp_send_buffer_write()
  C->>S: pbl_pp_send_buffer_end()
  S->>S: append to the send queue
  S->>M: schedule send_next (once)
  M->>T: send_next(session)
  T->>S: copy or peek queued bytes
  T->>L: send
  L-->>T: delivered (acknowledged)
  T->>S: pbl_pp_session_send_queue_consume()
  S->>S: free the buffer, wake waiting senders
```

When the caller is KernelMain, which runs `send_next`, waiting would never
end: beginning a buffer then sends the queue right away instead.

Sending may block, so it must not be done while holding a lock the
transports take, such as `bt_lock()`.

## Locking

The session layer has one lock. It protects the session list and each
session's send queue, flags and capabilities, and is never held while calling
a transport, a receiver's handler or a listener.

Each transport protects its own state with its own lock: `bt_lock()` for
PPoGATT, a mutex each for the console and QEMU. It may hold that lock while
calling into the session, so the lock order is always transport, then
session.

Since the session layer calls transports without its lock, a transport
callback may race with the transport closing the session. Callbacks therefore
receive the session rather than the transport's context: the transport looks
the session up among its own, under its own lock, and ignores the call if it
no longer owns it.
