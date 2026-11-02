# Boot

`subsys/boot/` is the firmware side of the interface with the bootloader. Its
public API is `include/pbl/boot/bootbit.h` for the boot bits and
`include/pbl/boot/version.h` for the bootloader version.

## Boot bits

The boot bits are one 32-bit retained register (`PBL_RETAINED_BOOTBITS`, see
`include/pbl/drivers/retained.h`) that the firmware and the bootloader both
read and write. Two bootloaders are in the field: PBLBOOT on SF32LB52 boards
and the legacy bootloader on nRF52 boards. Neither can be assumed to be
updated, so the bit positions are fixed:

| Bit | Name | Legacy | PBLBOOT | Firmware |
| --- | --- | --- | --- | --- |
| 0 | `INITIALIZED` | sets | sets | – |
| 1 | `NEW_FW_AVAILABLE` | installs and clears | – | sets after an update, clears on a core dump |
| 2 | `NEW_FW_UPDATE_IN_PROGRESS` | internal | – | – |
| 3–4 | `FW_START_FAIL_STRIKE_*` | counts | counts | skips the worker after two strikes |
| 5–6 | `RECOVERY_LOAD_FAIL_STRIKE_*` | counts | counts | – |
| 7 | `RECOVERY_START_IN_PROGRESS` | sets | sets | PRF clears |
| 9 | `SOFTWARE_FAILURE_OCCURRED` | counts a strike | counts a strike | sets on a crash |
| 11–13 | `RESET_LOOP_DETECT_*` | counts | counts | clears once up |
| 14 | `FW_STABLE` | clears the strikes | clears the strikes | sets after 15 minutes |
| 15 | `NEW_FW_INSTALLED` | internal | – | – |
| 17 | `FORCE_PRF` | starts PRF | starts PRF | sets |
| 18 | `NEW_PRF_AVAILABLE` | – | – | sets after an update, installs PRF on the next boot |

Bits 8, 10, 16 and 19 to 31 are unused; older firmware assigned some of them,
so new flags should start at bit 20. PBLBOOT declares the same layout in
`include/pb/bootbit.h`, and a change to either side has to be mirrored on the
other.

## Bootloader version

`pbl_boot_version_get()` reads the version PBLBOOT stores at the end of its
flash region, or the build timestamp the legacy bootloader leaves in the
`PBL_RETAINED_BOOT_VERSION` register.

The {apidoc}`API reference <subsys>` documents both interfaces.
