# Wire protocol

This page describes the Pebble protocol messages, independently of the link
that carries them. The definitions are in `include/pbl/pp/protocol.h`.

## Messages

A message is a 4-byte header followed by its payload. Header fields are big
endian:

| Offset | Size | Field |
|---|---|---|
| 0 | 2 | Payload length |
| 2 | 2 | Endpoint ID |
| 4 | length | Payload |

| Limit | Value | |
|---|---|---|
| `PBL_PP_MAX_INBOUND_PAYLOAD_PRIVATE` | 2044 bytes | From the Pebble mobile app |
| `PBL_PP_MAX_INBOUND_PAYLOAD_PUBLIC` | 144 bytes | From third party phone apps |
| `PBL_PP_MAX_OUTBOUND_PAYLOAD` | 656 bytes | To the phone |

Messages follow each other with nothing in between: the stream carries no
other framing, and a link may split it anywhere. Payloads are in each
endpoint's own format, most of them little endian.

## Endpoints

| ID | Name | Access | In PRF | Defined in |
|---|---|---|---|---|
| `0x0000` | Meta | any | yes | `subsys/pp/meta.c` |
| `0x000b` | Clock (time) | private | | `pebble/services/clock/service.c` |
| `0x0010` | System version | any | yes | `pebble/kernel/system_versions.c` |
| `0x0011` | Remote version | any | yes | `subsys/pp/version.c` |
| `0x0012` | System message | private | yes | `pebble/kernel/system_message.c` |
| `0x0020` | Music | private | | `pebble/services/music/endpoint.c` |
| `0x0021` | Phone | private | | `pebble/services/phone_pp/service.c` |
| `0x0030` | AppMessage | any | | `pebble/services/app_message/app_message_receiver.c` |
| `0x0031` | Launcher AppMessage | any | | `pebble/process_management/launcher_app_message.c` |
| `0x0032` | App customization | any | | `pebble/process_management/app_custom_icon.c` |
| `0x0033` | BLE control | private | | `pebble/services/bluetooth/pp_ble_control.c` |
| `0x0034` | App run state | any | | `pebble/process_management/app_run_state.c` |
| `0x0035` | Imaging | private | | `pebble/services/imaging/imaging.c` |
| `0x038f` | Health sync | private | | `pebble/services/health_sync_endpoint/service.c` |
| `0x07d1` | Ping | any | | `pebble/services/ping/service.c` |
| `0x07d2` | Log dump | private | yes | `pebble/services/log_dump_endpoint/service.c` |
| `0x07d3` | Reset | private | yes | `pebble/kernel/util/fw_reset.c` |
| `0x07d6` | App log | private | | `pebble/services/app_log_endpoint/service.c` |
| `0x1389` | Factory registry | private | yes | `pebble/services/registry_endpoint/service.c` |
| `0x1771` | App fetch | private | | `pebble/services/app_fetch_endpoint/service.c` |
| `0x1a7a` | Data logging | private | | `pebble/services/data_logging/dls_endpoint.c` |
| `0x1f40` | Screenshot | private | | `pebble/services/compositor/screenshot_pp.c` |
| `0x2328` | Get Bytes | private | yes | `pebble/services/get_bytes/get_bytes.c` |
| `0x2710` | Audio | private | | `pebble/services/audio_endpoint/service.c` |
| `0x2af8` | Voice | private | | `pebble/services/voice_endpoint/service.c` |
| `0x2cb0` | Timeline action | private | | `pebble/services/timeline/actions_endpoint.c` |
| `0xabcd` | App order | private | | `pebble/services/app_order_endpoint/service.c` |
| `0xb1db` | BlobDB | private | | `pebble/services/blob_db/endpoint.c` |
| `0xb2db` | BlobDB v2 | private | | `pebble/services/blob_db/endpoint2.c` |
| `0xbeef` | Put Bytes | private | yes | `pebble/services/put_bytes/put_bytes.c` |
| `0xcafe` | Poll remote | any | | `pebble/services/poll_remote/service.c` |
| `0xcaff` | xsbug | any | | `pebble/applib/moddable/moddable.c`, with moddable |
| `0xf00d` | Remote input | private | yes | `pebble/kernel/remote_input.c` |

*Private* endpoints only accept messages from the Pebble mobile app, on a
system or hybrid session; *any* endpoints also accept them from third party
phone apps. The watch sends on any endpoint, including ones it does not
receive on.

(pp-meta)=
## Meta endpoint

The watch answers a message it cannot take on the meta endpoint, and drops
its payload:

| Offset | Size | Field |
|---|---|---|
| 0 | 1 | Error |
| 1 | 2 | Endpoint ID of the message, big endian |

| Error | Value | Meaning |
|---|---|---|
| Disallowed | `0xdd` | The endpoint is private and the session is an app session |
| Unhandled | `0xdc` | No such endpoint in this firmware |
| Corrupted | `0xd0` | A malformed message; the response is the error byte alone |

Messages the watch accepts but has no room for, such as a large message while
the kernel heap is short, are dropped without a response.

(pp-version)=
## Version and capabilities

As soon as a session opens, the watch asks the phone for its version on the
remote version endpoint, `0x0011`. The phone's answer sets the session's
capabilities, which code checks before using features the phone may lack.
Transports that know their peer up front, the console and QEMU, set
capabilities when they open the session, and the answer replaces them.

```{mermaid}
sequenceDiagram
  participant W as Watch
  participant P as Phone
  Note over W: session opens
  W->>P: 0x0011 [0x00] version request
  P->>W: 0x0011 [0x01, response]
  Note over W: set capabilities,<br/>PBL_PP_SESSION_CAPABILITIES,<br/>PEBBLE_REMOTE_APP_INFO_EVENT (system only)
```

The phone asks for the watch's versions the same way, on the system version
endpoint, `0x0010`.

The response comes in three versions, told apart by their length. Each
extends the previous one:

| Field | Size | Since | |
|---|---|---|---|
| Library version | 4 | v1 | Deprecated |
| Session capabilities | 4 | v1 | Deprecated |
| Platform | 4 | v1 | Big endian; bits 0-2 are the phone OS |
| Response version | 1 | v2 | 2 |
| Major, minor, bugfix | 3 | v2 | Version of the mobile app |
| Protocol capabilities | 4 | v3 | Little endian bit field, see below |

The 1.x Android app repeats its response, so a v1 response may be as long as a
v2 one; it then has no capabilities. A response shorter than v1 is ignored.

The phone OS, `enum pbl_pp_remote_os`, is unknown (0), iOS (1), Android
(2), macOS (3), Linux (4) or Windows (5). It reaches the rest of the firmware
in `PEBBLE_REMOTE_APP_INFO_EVENT`.

### Capabilities

`enum pbl_pp_capability` names the bits the firmware checks;
`struct pbl_pp_capabilities` lays out all of them as the phone sends them.

| Bit | Capability | Phone supports |
|---|---|---|
| 0 | `PBL_PP_CAP_RUN_STATE` | The app run state endpoint |
| 1 | `PBL_PP_CAP_INFINITE_LOG_DUMPING` | Log dumps of any length |
| 2 | `PBL_PP_CAP_EXTENDED_MUSIC` | The extended music service |
| 3 | `PBL_PP_CAP_EXTENDED_NOTIFICATIONS` | The extended notification service |
| 4 | `PBL_PP_CAP_LANGUAGE_PACK` | Language pack installation |
| 5 | `PBL_PP_CAP_APP_MESSAGE_8K` | 8 KiB AppMessage buffers |
| 6 | `PBL_PP_CAP_ACTIVITY_INSIGHTS` | Activity insights |
| 7 | `PBL_PP_CAP_VOICE_API` | The voice API |
| 8 | `PBL_PP_CAP_SEND_TEXT` | Sending text |
| 9 | `PBL_PP_CAP_NOTIFICATION_FILTERING` | Notification filtering |
| 10 | `PBL_PP_CAP_UNREAD_COREDUMP` | Fetching unread core dumps |
| 11 | `PBL_PP_CAP_WEATHER_APP` | The weather app |
| 12 | `PBL_PP_CAP_REMINDERS_APP` | The reminders app |
| 13 | `PBL_PP_CAP_WORKOUT_APP` | The workout app |
| 14 | `PBL_PP_CAP_SMOOTH_FW_INSTALL_PROGRESS` | Smooth firmware install progress |
| 17 | `PBL_PP_CAP_IMAGING` | Serving images on the imaging endpoint |
| 23 | `PBL_PP_CAP_SETTINGS_SYNC` | Syncing settings through BlobDB |
| 26 | `PBL_PP_CAP_NOTIFICATION_WINDOW_STATE` | Tracking which notification the watch shows |

The Bluetooth service caches the system session's capabilities, so that code
can check them while the phone is away. The `app_session` service caches the
capabilities of each app's phone counterpart in the same way.
