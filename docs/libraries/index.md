# Libraries

Generic support code shared by the firmware, the bootloader and host builds
lives under `lib/`. It depends on no OS service, so it is also what the unit
tests link against directly.

- **Utilities**: `lib/util`, included as `pbl/util/<module>.h`; see
  {apidoc}`the API reference <util>`.
- **Kraepelin**: `lib/kraepelin`, the step, sleep and activity session
  detection algorithm the activity service runs, included as
  `pbl/kraepelin/kraepelin.h`; see {apidoc}`the API reference <kraepelin>`.
- **Fuel gauge**: `lib/fuel_gauge`, battery state-of-charge estimation,
  included as `pbl/fuel_gauge/fuel_gauge.h`; see [Fuel gauge](fuel_gauge/index.md).
- **Shared definitions**: headers under `include/pebbleos/` describe data
  exchanged with the bootloader and host tooling, so their layout is fixed;
  see {apidoc}`the API reference <pebbleos>`.

```{toctree}
:hidden:

fuel_gauge/index.md
```
