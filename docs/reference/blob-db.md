# BlobDB endpoint

BlobDB is the key-value store the phone uses to push data to the watch
(notifications, timeline pins, app metadata, weather, settings, …). Two
Pebble Protocol endpoints speak it: `0xb1db` for phone-initiated writes
(`src/fw/services/blob_db/endpoint.c`) and `0xb2db` for watch-initiated
sync (`src/fw/services/blob_db/endpoint2.c`). Command opcodes and
response codes are in `include/pbl/services/blob_db/endpoint_private.h`,
database ids in `include/pbl/services/blob_db/api.h`. All multi-byte
fields are little-endian.

## Commands (endpoint `0xb1db`)

Every message starts with a `uint8_t` command and a `uint16_t` token the
watch echoes in its response. Keys and values are length-prefixed:
`uint8_t key_size` + key bytes, `uint16_t value_size` + value bytes.

| Command               | Opcode | Body after token                              |
| --------------------- | ------ | --------------------------------------------- |
| INSERT                | `0x01` | database id (`uint8_t`), key, value           |
| DELETE                | `0x04` | database id, key                              |
| CLEAR                 | `0x05` | database id — drops all entries               |
| INSERT_WITH_TIMESTAMP | `0x0D` | database id, `uint32_t` timestamp, key, value |

`0x02` (READ) and `0x03` (UPDATE) are defined but not implemented.
INSERT_WITH_TIMESTAMP is for conflict resolution: if the watch's copy is
newer, it answers DATA_STALE and the phone should re-sync. The timestamp
is currently only honored for the Settings database; other databases
treat it as a plain INSERT.

The watch responds to every command with `uint16_t token` +
`uint8_t response code`:

| Code   | Meaning                                       |
| ------ | --------------------------------------------- |
| `0x01` | SUCCESS                                       |
| `0x02` | GENERAL_FAILURE                               |
| `0x03` | INVALID_OPERATION                             |
| `0x04` | INVALID_DATABASE_ID                           |
| `0x05` | INVALID_DATA                                  |
| `0x06` | KEY_DOES_NOT_EXIST                            |
| `0x07` | DATABASE_FULL                                 |
| `0x08` | DATA_STALE                                    |
| `0x09` | DB_NOT_SUPPORTED                              |
| `0x0A` | DB_LOCKED                                     |
| `0x0B` | TRY_LATER (endpoint not accepting writes yet) |

## Sync commands (endpoint `0xb2db`)

Opcodes `0x06`–`0x0C` (DIRTY_DBS, START_SYNC, WRITE, WRITEBACK,
SYNC_DONE, VERSION, DIRTY_ALL) implement watch→phone writeback of dirty
records; responses to these set bit 7 of the opcode. VERSION reports the
protocol version (currently 1). Message layouts are the packed structs
in `endpoint2.c`.

## Database ids

| Id     | Database     | Id     | Database      |
| ------ | ------------ | ------ | ------------- |
| `0x00` | Test         | `0x07` | Prefs         |
| `0x01` | Pins         | `0x08` | Contacts      |
| `0x02` | Apps         | `0x09` | WatchAppPrefs |
| `0x03` | Reminders    | `0x0A` | Health        |
| `0x04` | Notifs       | `0x0B` | AppGlance     |
| `0x05` | Weather      | `0x0C` | Settings      |
| `0x06` | iOSNotifPref |        |               |

New databases register a `BlobDBId` and an entry in `s_blob_dbs` in
`src/fw/services/blob_db/api.c`.
