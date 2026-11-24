# Shared definitions

Headers under `include/pebbleos/` describe data exchanged between the
firmware, the bootloader and host tooling, such as the firmware metadata at
the end of every image and the processor IDs. Their layout is fixed.

| | |
| --- | --- |
| Header | `pebbleos/firmware_metadata.h`, `pebbleos/chip_id.h` |
| API | {apidoc}`API reference <pebbleos>` |
