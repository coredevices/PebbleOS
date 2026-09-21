# Settings dictionary

Status: proposal, open for discussion.

A build-generated, self-describing dictionary of watch settings, modelled on
the CANopen object dictionary. The firmware is the single source of truth for
what a setting is; the phone learns the set of settings, their types and how to
present them from the watch itself, so adding a setting no longer requires
mobile app changes.

## Motivation

Today a syncable preference is declared in five places, and the wire format is
implied by convention rather than stated anywhere:

- Firmware: key define and static in `src/fw/shell/normal/prefs.c`, an entry
  in `prefs_values.h.inc`, a `prv_set_*` handler, and the whitelist string in
  `src/fw/services/blob_db/settings_blob_db.c`. Notification preferences live
  in a second file (`alerts_preferences.c`) with a different key convention
  (no trailing NUL), which the sync layer has to canonicalise.
- libpebble3: `WatchPrefEntity.kt` re-declares every key, type, default, label
  and description, plus every enum's numeric codes, plus per-platform hacks
  such as the text-size offset for emery and gabbro.
- CoreApp: the settings screens map each preference to a section and widget
  by hand.
- iOS Companion: `WatchPreferences.swift` repeats a subset.

The phone has no way to learn what a given watch supports, so board
differences become phone-side special cases and unknown keys are silently
dropped. The sync itself is a bidirectional BlobDB (`BlobDBIdSettings`) with
per-record timestamps and dirty lists, which is more machinery than settings
need.

## Overview

The feature is new and independent. It does not replace the existing shell
prefs or BlobDB sync; legacy preferences may migrate later, one group at a
time, or never.

Three pieces:

1. **Dictionary** (`settings.yaml`): the data contract. Numeric ids, types,
   wire encoding, hard bounds, defaults, access, Kconfig gating. Owned by
   firmware. Contains no presentation.
2. **UI schemas** (`ui/mobile.yaml`, `ui/watch.yaml`): how a human sees each
   entry. Grouping, labels, widget choice, display units, visibility. Can be
   owned by app or design people. Reference dictionary entries by name and
   never restate types.
3. **Generator**: one build step that validates both against each other and
   emits a C table plus typed accessors for the firmware, and CBOR blobs of the
   dictionary and each UI schema that the watch serves to the phone.

The watch is authoritative for values. The phone reads all values on connect,
subscribes to change notifications, and sends writes as commands the watch
validates and either applies or rejects. There are no timestamps, no dirty
lists and no conflict resolution.

## Dictionary

Every entry has a stable `u16` id assigned in the YAML. Ids are never reused;
a removed entry keeps its id reserved. The wire format is `id, len, bytes`,
the storage key is two bytes, and the string-key length ambiguity of the
current sync goes away.

```yaml
schema: 1
entries:
  - id: 0x1000
    name: backlight_enabled
    type: bool
    default: true
  - id: 0x1001
    name: backlight_intensity
    type: u8
    range: {min: 1, max: 100}
    default: 25
    access: rw
  - id: 0x1002
    name: backlight_timeout
    type: u32
    unit: ms
    range: {min: 1000, max: 15000}
    default: 3000
  - id: 0x1003
    name: backlight_color
    type: rgb
    default: board          # filled from BOARD_CONFIG at init
    if: CONFIG_BACKLIGHT_HAS_COLOR
  - id: 0x1004
    name: text_size
    type: enum
    base: u8
    values: {0: smaller, 1: default, 2: larger}   # machine names only
    values_if:
      CONFIG_LARGE_TEXT_TIERS: {1: smaller, 2: default, 3: larger}
  - id: 0x1010
    name: quick_launch_up
    type: app_ref
  - id: 0x2001
    name: heart_rate
    type: struct
    fields:
      - {name: resting, type: u8, range: {min: 30, max: 220}}
      - {name: elevated, type: u8}
      - {name: max, type: u8}
  - id: 0x2002
    name: activation_timestamp
    type: i32
    access: watch_only      # stored, never exported
```

Fields:

| Field | Meaning |
| --- | --- |
| `id` | Stable `u16`, unique, never reused |
| `name` | Identifier used by generated C, UI schemas and translations |
| `type` | One of the types below |
| `base` | Wire encoding for `enum` and `flags` (`u8`, `u16`, `u32`) |
| `range` | Hard bounds enforced by the firmware validator |
| `values` | Enum or flag tokens, `code: name`; per-board variants via `values_if` |
| `unit` | Semantic unit of a number (`ms`, `percent`, `bpm`, ...) |
| `default` | Literal, or `board` to take the value from board config at init |
| `access` | `rw` (default), `ro` (exported, not writable), `watch_only` |
| `if` | Kconfig symbol; entry exists only when set |
| `since` | Dictionary schema version the entry was added in (informational) |

Types and their encodings. All integers are little-endian.

| Type | Encoding |
| --- | --- |
| `bool` | `u8`, 0 or 1 |
| `u8` `i8` `u16` `i16` `u32` `i32` | as named |
| `enum` | `base` integer, must be one of `values` |
| `flags` | `base` integer, bitwise OR of `values` |
| `string` | UTF-8, NUL-terminated, `max_len` bytes including NUL |
| `uuid` | 16 bytes |
| `app_ref` | `u8` enabled + 16-byte UUID |
| `color` | `u8` Pebble 64-colour value |
| `rgb` | `u32`, `0x00RRGGBB` |
| `schedule` | packed weekday mask + start/end minutes, see generated header |
| `struct` | packed `fields` in declaration order, each a scalar type above |

Constraints in the dictionary are semantic: they are what the firmware
enforces on every write regardless of origin. Presentation choices such as
"show four presets instead of a 1..100 slider" belong in a UI schema.

## UI schemas

A UI schema lists what to show, in what order, with what widget. It references
entries by `name`. There is one per surface; the phone one is served to the
mobile app, the watch one is compiled into a C table for the on-watch Settings
app.

```yaml
schema: 1
groups:
  - name: display
    label: Display
    items:
      - entry: backlight_enabled
        label: Backlight
      - entry: backlight_intensity
        label: Backlight intensity
        widget: choice
        options: {10: Low, 25: Medium, 50: High, 100: Blinding}
      - entry: backlight_timeout
        label: Backlight timeout
        widget: slider
        step: 500
        display_unit: s
        visible_if: backlight_enabled == true
      - entry: text_size
        label: Text size
        widget: segmented
        labels: {smaller: Smaller, default: Default, larger: Larger}
      - entry: heart_rate
        label: Heart rate zones
        widget: custom:heart_rate_zones
  - name: developer
    label: Developer
    debug: true
    items:
      - entry: backlight_timeout   # same entry, raw number instead of slider
        widget: number
```

Widgets: `toggle`, `choice`, `segmented`, `list`, `slider`, `stepper`,
`number`, `text`, `color`, `app_picker`, `schedule`, `form` (for structs),
and `custom:<name>`. A renderer that knows a custom name renders its own
component; one that does not falls back to the type-derived default. Unknown
widgets or a newer UI schema version degrade the same way, so an old app
tolerates new firmware.

Rules the generator enforces at build time:

- Every item references an existing dictionary entry present for the board.
- Every `options` or `labels` key is a valid value of the entry. A slider or
  stepper `step` fits the entry's range. `display_unit` is convertible from
  the entry's `unit`.
- `widget` is optional. When omitted it is derived from the type: `bool` to
  `toggle`, `enum` to `list`, `flags` to multi-select `list`, bounded integer
  to `stepper`, unbounded integer to `number`, `struct` to `form`.
- An entry absent from a UI schema is not shown on that surface. Exposure is
  explicit, so `watch_only` or internal entries cannot leak into a screen.
- Labels are keyed by entry name, which is where gettext (watch) or the app's
  translation tables (phone) hook in. The blobs ship English labels as a
  fallback.

## Generated firmware side

Layout:

```
subsys/settings/settings.yaml           dictionary
subsys/settings/ui/mobile.yaml          phone presentation
subsys/settings/ui/watch.yaml           on-watch presentation
subsys/settings/                        core: table lookup, storage, validation, observers
include/pbl/settings/settings.h         public API
src/fw/services/settings_proto/         Pebble Protocol endpoint (comm-dependent)
tools/cmake/settings.py                 generator
```

The generator emits `settings.auto.h` and `settings.auto.c`:

- `enum settings_id` and a const table: id, type, size, default, range or
  values, access, gated with `#ifdef` from `if`.
- Typed accessors so services stop hand-writing getters, for example
  `settings_get_backlight_timeout()` returning `uint32_t` and
  `settings_set_backlight_intensity(uint8_t)`. These wrap the generic
  `settings_get(id, buf, len)` and `settings_set(id, buf, len)`.
- A per-board CBOR blob of the dictionary and of each UI schema, embedded as
  system resources, each with its SHA-256. The firmware never parses the
  blobs; it serves them.

Core API:

```c
int settings_get(uint16_t id, void *buf, size_t len);
int settings_set(uint16_t id, const void *buf, size_t len);
int settings_reset(uint16_t id);

typedef int (*settings_validator_t)(uint16_t id, const void *buf, size_t len);
typedef void (*settings_observer_t)(uint16_t id, void *ctx);
void settings_add_validator(uint16_t id, settings_validator_t fn);
void settings_add_observer(uint16_t id, settings_observer_t fn, void *ctx);
```

Every `settings_set` is validated from the table first: size, range, enum or
flag membership, struct field ranges. Hand-written C is only needed for
cross-field invariants (`resting <= elevated <= max`) and side effects (poke
the HRM manager), registered as validators and observers. Observers fire for
both local and phone-originated writes.

Storage is one settings file keyed by id. A missing record means the default,
so adding an entry needs no migration step. The core has no dependency on the
comm stack and is unit-tested natively.

## Protocol

A new Pebble Protocol endpoint. The watch advertises a
`settings_dictionary_support` capability bit.

| Op | Direction | Payload |
| --- | --- | --- |
| `DICT_INFO` | phone to watch | reply: schema version, dictionary hash and length, UI schema hash and length, entry count |
| `READ_ALL` | phone to watch | reply: every exported `id, len, value` |
| `READ` | phone to watch | `id`; reply: `id, len, value` |
| `WRITE` | phone to watch | `id, len, value`; reply: `id, status` (`ok`, `invalid`, `read_only`, `unknown`) |
| `CHANGED` | watch to phone | `id, len, value`, on any local or remote change |

The dictionary and UI blobs are fetched through the existing get_bytes
service with a new object type, which already handles chunking; the hashes
from `DICT_INFO` let the phone cache them and fetch once per firmware update.
The same blobs are included in the firmware bundle so the app can prefetch
them before an OTA and render the new screen immediately after reboot.

On connect the phone checks the hashes, fetches blobs if needed, issues
`READ_ALL` and then applies `CHANGED` messages to a display cache. An edit
made while disconnected is a queued `WRITE` that is applied or rejected on
reconnect. The phone never holds authoritative state.

## Mobile side

libpebble3 gains a CBOR decoder for the two blobs and one renderer that maps
groups to widgets by type. There is no per-setting Kotlin or Swift. The
existing `WatchPref*` enums, offset hacks and hand-written screens are not
touched by this feature; they cover the legacy BlobDB path until it is
retired.

The UI schema is advisory. The app may override it for a given dictionary
hash, and a server could push a refreshed layout without a firmware update,
because the dictionary alone fixes the data contract.

## On-watch settings

Because the watch UI schema carries labels, widgets and ranges for the same
table, the on-watch Settings app can render generic entries from it. Adding a
plain toggle then becomes a one-line change in `settings.yaml` plus one item
in each UI schema, with no C and no mobile work.

## Rollout

1. **Subsystem only.** `subsys/settings`, the YAML files, generator, typed
   API, validators, observers, unit tests. The first new feature that needs a
   setting uses it. Existing prefs are untouched.
2. **Transport.** Endpoint, capability bit, get_bytes object type, CBOR
   resources, hash check, libpebble3 renderer. From here on, new settings need
   no mobile work.
3. **Optional migration.** Move legacy preferences one group at a time with a
   one-shot import hook that reads the old key, and let `BlobDBIdSettings` and
   its whitelists shrink until they can go.

## Open questions

- Blob transport: get_bytes object type (reuses chunking) versus chunked reads
  on the new endpoint (self-contained). The draft assumes get_bytes.
- Label ownership: English in the YAML with translation via gettext on the
  watch and app-side tables on the phone, or a separate strings file per
  locale in the repo.
- Whether phone writes to entries shown only on debug pages should require a
  developer-mode flag on the watch.
- Struct fields versus flattening every struct into scalar entries. Scalars
  are simpler on the wire; structs keep related fields atomic and let a
  validator see them together.
