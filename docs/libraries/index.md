# Libraries

Generic support code shared by the firmware, the bootloader and host builds
lives under `lib/`. It depends on no OS service, so it is also what the unit
tests link against directly.

```{apigroups} lib
```

## Utilities

`lib/util`, included as `pbl/util/<module>.h`:

```{apigroups} util
```

## Shared definitions

Headers under `include/pebbleos/` describe data exchanged with the
bootloader and host tooling, so their layout is fixed:

```{apigroups} pebbleos
```
