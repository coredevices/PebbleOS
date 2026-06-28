# Getafix (Pebble Round 2)

## Programming

Getafix(Pebble Round 2) has a B2B (Board-To-Board) connector that gives access to:
- MCU VDD, VUSB and GND
- RESET
- Flashing/Debug UART RX/TX

The pinout is as shown below.

```{figure} images/pbl-prog.webp
Getafix(Pebble Round 2) Programming pinout
```

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