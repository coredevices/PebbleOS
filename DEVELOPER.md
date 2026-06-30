# PebbleOS Developer Guide

## Resource Build, Packaging, and Flash Architecture

### Overview

This document describes how resources are built, packaged, and flashed on PebbleOS, with specific details for the SF32LB52 (Obelix/Getafix) platform.

---

## Flash Memory Map (SF32LB52 / GD25Q256E - 32MB)

The flash memory layout is defined in `src/fw/flash_region/flash_region_gd25q256e.h`.

**Base Address:** `0x12000000`

| Region                  | Size    | Start Address | End Address   | Description |
|-------------------------|---------|---------------|---------------|-------------|
| PTABLE                  | 64K     | 0x12000000    | 0x1200FFFF    | Partition table |
| BOOTLOADER              | 64K     | 0x12010000    | 0x1201FFFF    | Bootloader (pblboot) |
| FIRMWARE_SLOT_0         | 3072K   | 0x12020000    | 0x1231FFFF    | Primary firmware |
| FIRMWARE_SLOT_1         | 3072K   | 0x12320000    | 0x1261FFFF    | Secondary firmware (A/B update) |
| SYSTEM_RESOURCES_BANK_0 | 2048K   | 0x12620000    | 0x1281FFFF    | Resources bank A |
| SYSTEM_RESOURCES_BANK_1 | 2048K   | 0x12820000    | 0x12A1FFFF    | Resources bank B (backup) |
| SAFE_FIRMWARE (PRF)     | 576K    | 0x12A20000    | 0x12AAFFFF    | Recovery firmware |
| FILESYSTEM              | 21056K  | 0x12AB0000    | 0x13F3FFFF    | User filesystem |
| CD (core dump)          | 512K    | 0x13F40000    | 0x13FBFFFF    | Core dump storage |
| DEBUG_DB                | 128K    | 0x13FCF000    | 0x13FEEFFF    | Debug database |
| MFG_RESULTS             | 4K      | 0x13FFB000    | 0x13FFBFFF    | Manufacturing results |
| MFG_BATTERY_STATE       | 4K      | 0x13FFC000    | 0x13FFCFFF    | Battery state |
| TZINFO                  | 4K      | 0x13FFD000    | 0x13FFDFFF    | Timezone info |
| MFG_INFO                | 4K      | 0x13FFE000    | 0x13FFEFFF    | Manufacturing info |
| SHARED_PRF_STORAGE      | 4K      | 0x13FFF000    | 0x13FFFFFF    | Shared PRF storage |

**Total Flash Size:** 32MB (0x2000000)

---

## Resource Build System

### Resource Map Files

Resources are defined in JSON files under `resources/`:

```
resources/
├── common/
│   ├── base/resource_map.json          # Common resources (all platforms)
│   └── <board_family>/resource_map.json # Board family specific
├── normal/
│   ├── base/resource_map.json          # Normal build resources
│   ├── base/lang/                      # Language packs
│   │   ├── en_US/lang_map.json
│   │   ├── zh_CN/lang_map.json
│   │   └── ...
│   └── <board_family>/resource_map.json # Platform specific (e.g., obelix)
└── prf/
    └── base/resource_map.json          # Recovery build resources
```

### Build Flow

The resource build is defined in `resources/wscript_build`:

```
resource_map.json files
        │
        ▼
┌─────────────────────────────────────┐
│  Merge resource maps:               │
│  - common/base                      │
│  - normal/base or prf/base          │
│  - common/<board_family>            │
│  - normal/<board_family>            │
└─────────────────────────────────────┘
        │
        ▼
┌─────────────────────────────────────┐
│  Generate Resource Ball             │
│  (system_resources.resball)         │
│  - Process fonts (TTF, BDF, PBF)    │
│  - Process images (PNG, PDC, PBI)   │
│  - Process raw files                │
│  - Process vibe patterns            │
└─────────────────────────────────────┘
        │
        ▼
┌─────────────────────────────────────┐
│  Generate PBPack                    │
│  (system_resources.pbpack)          │
│  - Binary packed format             │
│  - Deduplication of identical data  │
└─────────────────────────────────────┘
        │
        ├──▶ resource_ids.auto.h      (Resource ID definitions)
        ├──▶ builtin_resources.auto.c (Builtin resources linked into FW)
        ├──▶ font_resource_keys.auto.h (Font key table)
        └──▶ timeline_resource_table.auto.c (Timeline resources)
```

### Generated Artifacts

| File | Description |
|------|-------------|
| `build/system_resources.pbpack` | Packed resource binary (flashed to device) |
| `build/src/fw/resource/resource_ids.auto.h` | Resource ID enum definitions |
| `build/src/fw/builtin_resources.auto.c` | Builtin resources compiled into firmware |
| `build/src/fw/font_resource_keys.auto.h` | Font resource key mappings |
| `build/src/fw/font_resource_table.auto.h` | Font resource table |
| `build/src/fw/resource/timeline_resource_table.auto.c` | Timeline pin resources |
| `build/src/fw/resource/pfs_resource_table.auto.c` | Filesystem resources |

### Build Commands

```bash
# Build firmware and resources
./pbl build

# Build only SDK
./pbl configure --board obelix_dvt
./pbl build --onlysdk
```

---

## Resource Types

Defined in `tools/resources/resource_map/`:

| Type | Generator | Description |
|------|-----------|-------------|
| `font` | `resource_generator_font.py` | Fonts (TTF, BDF, PBF) |
| `png` | `resource_generator_png.py` | PNG images |
| `pdc` | `resource_generator_pdc.py` | Pebble Draw Commands (vector) |
| `pbi` | `resource_generator_pbi.py` | Pebble Bitmap Image |
| `raw` | `resource_generator_raw.py` | Raw binary data |
| `vibe` | `resource_generator_vibe.py` | Vibration patterns |
| `mo` | `resource_generator_mo.py` | Translation catalogs (gettext) |

---

## Language Packs (i18n)

### Structure

Language packs are stored in `resources/normal/base/lang/<locale>/`:

```
resources/normal/base/lang/
├── en_US/
│   ├── lang_map.json    # Resource mapping
│   └── (empty - uses default fonts)
├── zh_CN/
│   ├── lang_map.json    # Chinese resource mapping
│   └── tintin.po        # Translation strings
├── de_DE/
├── fr_FR/
└── ...
```

### lang_map.json Format

```json
{
    "strings": {
        "lang": "en_CN",           // ISO locale code
        "name": "STRINGS",
        "file": "tintin.po"        // Translation file (or "" for default)
    },
    "fonts": [
        {
            "name": "GOTHIC_14_EXTENDED",
            "file": "fireflyR14.bdf",           // Font file for this locale
            "characterList": "notification_codepoints.json",
            "extended": true
        },
        // ... more fonts with aliases
    ],
    "images": []
}
```

### Build Language Pack

```bash
# Generate/update translation files for a language
./pbl make_lang --lang zh_CN

# Build a single language pack
./pbl pack_lang --lang zh_CN

# Build all language packs
./pbl pack_all_langs
```

Output: `build/resources/normal/base/lang/<locale>/<locale>.pbl`

---

## Flashing Resources

### Flash Addresses (SF32LB52)

```python
# From tools/sftool_flash_imaging.py
SYSTEM_RESOURCES_ADDR = 0x12620000   # Resources bank 0
SAFE_FIRMWARE_ADDR = 0x12A20000      # Recovery firmware (PRF)
```

### Flash Commands

#### Using pbl CLI

```bash
# Flash firmware only
./pbl flash --tty /dev/ttyACM0

# Flash firmware + resources
./pbl flash --tty /dev/ttyACM0 --resources

# Flash resources only
./pbl image_resources --tty /dev/ttyACM0

# Flash recovery firmware
./pbl image_recovery --tty /dev/ttyACM0
```

#### Using sftool directly

```bash
# Flash resources
sftool -c SF32LB52 -p /dev/ttyACM0 write_flash build/system_resources.pbpack@0x12620000

# Flash recovery firmware
sftool -c SF32LB52 -p /dev/ttyACM0 write_flash build/src/fw/tintin_fw.bin@0x12A20000
```

### Dual-Bank Resource Updates

Resources support A/B banking for atomic updates:
- **Bank 0:** `0x12620000` - 2MB
- **Bank 1:** `0x12820000` - 2MB

The system can switch between banks, enabling:
- Atomic resource updates
- Rollback on failed updates
- Safe OTA updates

---

## Firmware Memory Layout

### SF32LB52 Linker Script

Defined in `src/fw/sf32lb52_flash_fw.ld.template`:

```
MEMORY
{
    KERNEL_RAM  (xrw) : ORIGIN = @KERNEL_RAM_ADDR@, LENGTH = @KERNEL_RAM_SIZE@
    WORKER_RAM (xrw)  : ORIGIN = @WORKER_RAM_ADDR@, LENGTH = @WORKER_RAM_SIZE@
    APP_RAM (xrw)     : ORIGIN = @APP_RAM_ADDR@,    LENGTH = @APP_RAM_SIZE@
    FLASH (rx)        : ORIGIN = @FW_FLASH_ORIGIN@, LENGTH = @FW_FLASH_LENGTH@
}
```

### Flash Layout Calculation (from `src/fw/wscript`)

```python
# SF32LB52 sizes
flash_size = 32 * 1024 * 1024      # 32MB total
ptable_size = 64 * 1024            # 64K partition table
bootloader_size = 64 * 1024        # 64K bootloader
slot_size = 3072 * 1024            # 3MB firmware slot
resources_size = 2048 * 1024       # 2MB resource bank
prf_size = 576 * 1024              # 576K recovery firmware

# Normal firmware offset
offset_size = ptable_size + bootloader_size
# For SLOT 1: add slot_size

# Recovery firmware offset
offset_size = ptable_size + bootloader_size + 2*slot_size + 2*resources_size
```

### RAM Layout

```python
# Platform-specific RAM sizes
APP_RAM_SIZES = {
    'emery': AppRamSize(130 * 1024, 62 * 1024),   # 512K SRAM
    'flint': AppRamSize(66 * 1024, 30 * 1024),    # 256K SRAM
    'gabbro': AppRamSize(130 * 1024, 94 * 1024),  # 512K SRAM
}

# RAM allocation (from end to start)
worker_ram_size = 12 * 1024  # 12K for worker
kernel_ram_size = total - app_ram_size - worker_ram_size
```

---

## Build Configuration

### Configure Options

```bash
# Basic configure
./pbl configure --board obelix_dvt

# Release build
./pbl configure --board obelix_dvt -DCONFIG_RELEASE=y

# Manufacturing mode
./pbl configure --board obelix_dvt --mfg

# Recovery firmware variant
./pbl configure --board obelix_dvt --variant=prf

# Enable verbose logging
./pbl configure --board obelix_dvt --nohash --log-level=debug
```

### Build Variants

| Variant | Description |
|---------|-------------|
| `normal` | Standard firmware with all features |
| `prf` | Recovery firmware (minimal, for safe mode) |

---

## File Format Reference

### PBPack Format

The `.pbpack` format is a binary container for resources:
- Header with resource count and manifest
- Resource entries with CRC32 checksums
- Deduplication via content-addressed storage

### Resource IDs

Generated from `resource_map.json` entries:
```c
// resource_ids.auto.h
enum {
    RESOURCE_ID_FONT_GOTHIC_14 = 0,
    RESOURCE_ID_FONT_GOTHIC_18 = 1,
    // ...
    RESOURCE_ID_IMAGE_MY_ICON = 42,
};
```

---

## Troubleshooting

### Resource Too Large

If resources exceed the 2MB bank size:
```
Resources are too large for target board 2097152 > 2097152
```

Solutions:
1. Remove unused resources from `resource_map.json`
2. Use `"builtin": false` for optional resources
3. Compress images using PDC format

### Missing Language Pack

If language fonts are missing:
```bash
# Check if font files exist in resources/normal/base/lang/<locale>/
ls resources/normal/base/lang/zh_CN/

# Build the language pack
./pbl pack_lang --lang zh_CN
```

### Flash Write Failures

Common issues:
1. **Wrong tty port:** Check `ls /dev/ttyACM*` or `ls /dev/ttyUSB*`
2. **Permission denied:** Add user to `dialout` group (Linux)
3. **Chip not in bootloader:** Hold BACK button while resetting

---

## Related Files

| File | Description |
|------|-------------|
| `src/fw/flash_region/flash_region_gd25q256e.h` | SF32LB52 flash layout |
| `src/fw/wscript` | Firmware build configuration |
| `resources/wscript_build` | Resource build system |
| `tools/sftool_flash_imaging.py` | SF32LB52 flash tool |
| `pbl` | Main CLI entry point |
| `waftools/ldscript.py` | Linker script generation |
