# Language pack management

The watch stores up to four language packs. One of them, or built-in English,
provides the UI translation; every installed pack contributes font glyphs (see
{ref}`Installed language packs <installed-language-packs>`). Watches
advertise everything on this page with protocol capability bit 28
(`multiple_language_packs_support`). Older watches, and phone apps that don't
know the bit, keep using the single-pack flow. All multi-byte fields are
little-endian.

## Storage

Pack files use the existing pack format. Slot zero is the legacy `lang` file;
slots 1 to 3 are `lang1`, `lang2` and `lang3`
(`fw/resource/language_pack.h`). A slot is occupied by any file, including one
whose metadata can't be read.

## Installing

Packs are installed with an ordinary PutBytes file transfer:

- `lang-add` stores the pack in the first empty slot. If every slot is
  occupied, the transfer is rejected at initialization and no pack is
  replaced. Installing the same locale again takes another slot, so remove the
  old pack before installing an update. The active language doesn't change.
- `lang` (legacy) replaces slot zero and, once the transfer completes,
  selects it.

## System messages

Pack management uses the System Message endpoint (`0x12`,
`fw/kernel/system_message.c`). Every message starts with a zero byte and a
message type, followed by a one-byte token the watch echoes in its response.
Request and response structs are in `fw/kernel/language_pack_protocol.h`.

| Message                  | Type   | Direction      |
| ------------------------ | ------ | -------------- |
| List packs               | `0x0d` | phone → watch  |
| List packs response      | `0x0e` | watch → phone  |
| Remove pack              | `0x0f` | phone → watch  |
| Remove pack response     | `0x10` | watch → phone  |
| Select language          | `0x11` | phone → watch  |
| Select language response | `0x13` | watch → phone  |

`0x12` is not used, to avoid confusion with the endpoint ID.

### List packs

The request carries only the token. The response holds the token, the slot of
the active translation (`0xff` for English) and four 40-byte records, one per
slot:

| Field   | Size | Description                                          |
| ------- | ---- | ---------------------------------------------------- |
| slot    | 1    | Slot index                                           |
| flags   | 1    | bit 0 present, bit 1 readable, bit 2 translation     |
| version | 2    | Pack version, translation packs only                 |
| locale  | 6    | ISO locale, zero-padded, translation packs only      |
| name    | 30   | Native language name, zero-padded, translation only  |

A pack is readable if it carries a translation or at least one font.

### Remove pack and select language

Both requests hold the token, a slot, and the expected version and locale of
the pack in that slot. Packs without a translation use zero version and
locale; so does built-in English, which is selected with slot `0xff`. The
expected metadata stops a stale list from acting on a different pack:
mismatching requests fail.

Selecting requires a pack with a translation and keeps every installed pack.
Removing the selected pack selects English, even while English is toggled on.
Removing a pack that a file transfer is still writing fails.

Both responses hold the token, the slot and a success byte. Requests with
too-short payloads or invalid slots are answered with a failure. Additional
trailing fields are ignored for compatibility with future phone apps.
