# CRC

`subsys/crc/` computes every checksum the firmware stores or exchanges. Its
public API is `include/pbl/crc/crc.h`:

| Function | Algorithm | Used by |
| --- | --- | --- |
| `pbl_crc8()`, `pbl_crc8_reversed()` | CRC-8, polynomial 0x2F (CRC-8/OPENSAFETY) | PFS page chaining, settings file key hashes |
| `pbl_crc32()` | CRC-32/ISO-HDLC, zlib-compatible | PULSE2, flash CRC, shared PRF storage, boot bits |
| `pbl_crc32_legacy_*()` | CRC-32/MPEG-2 over little-endian words | PFS, app images, resources, data logging, PULSE, put bytes |

The legacy checksum reproduces what the STM32F2/F4 CRC unit computed when the
firmware fed it 32-bit words, including the zero padding of a trailing partial
word. It lives on in on-flash and on-wire formats, so its definition cannot
change; `crc.h` spells it out.

## Layering

The layering follows Linux's `lib/crc`. The public functions own the
software implementations, which are always built, and decide when to hand
work to hardware:

```{mermaid}
flowchart TD
  api["pbl_crc32() / pbl_crc32_legacy_update()<br/>subsys/crc"]
  hw["pbl_crc_hw_*()<br/>driver, optional"]
  sw["Software<br/>nybble-wide tables"]
  api -- "len ≥ CONFIG_CRC_HW_MIN_LEN" --> hw
  api -- "remainder" --> sw
  hw -. "what it did not process" .-> sw

  classDef app fill:#fa4a36,stroke:#fa4a36,color:#ffffff
  classDef system fill:#fd8a7c,stroke:#fd8a7c,color:#171717
  classDef base fill:#525252,stroke:#525252,color:#ffffff
  class api app
  class sw system
  class hw base
```

Hardware acceleration comes from a {doc}`CRC driver <../drivers/crc/index>`,
which may process any part of a buffer or none of it: the subsystem always
finishes in software, so callers never see a failure.

`CONFIG_CRC_HW_MIN_LEN` keeps short buffers in software, where programming the
peripheral costs more than it saves. CRC-8 inputs are a few bytes long and
are never offloaded.
