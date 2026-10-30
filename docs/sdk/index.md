# Application SDK

Watchfaces, watchapps and background workers are third-party programs,
built outside this repository against the application SDK. The SDK is the
contract between them and the firmware: the API apps can use, and the
binary interface through which they call it.

The SDK is generated from the firmware, by the same build:

- `pebble.h` and `pebble_worker.h` declare the API: the functions, types
  and constants of `fw/applib/` that are exported, per platform.
- `libpebble.a` is what apps link against. It holds no implementation,
  only one small trampoline per exported function, which jumps into the
  firmware.
- The app project templates, the resource tools and the CMake package app
  developers build with ({doc}`app_build`) complete the distribution, along
  with the deprecated `waf` build.

App developers do not use these files directly: the
[`pebble` tool](https://github.com/coredevices/pebble-tool) downloads a
released SDK and builds apps with it.

## Platforms

An app is built once per platform it supports, and each build sees the
SDK for that platform: its display size, shape and color depth, and the
functions only some platforms have.

| Platform | Display | Watches |
| --- | --- | --- |
| Flint | 144 × 168, black and white | Pebble 2 Duo |
| Emery | 200 × 228, color | Pebble Time 2 |
| Gabbro | 260 × 260 round, color | Pebble Round 2 |

## Versions and compatibility

Every app records the SDK version it was built with, a major and a minor
number, in its header. The firmware runs an app when the major matches its
own and the minor is not newer: apps built with an older SDK keep working
on newer firmware, while an app built with a newer SDK is refused, as it
may call functions the firmware does not have.

Each SDK version adds exported functions without moving the existing ones,
so new firmware stays compatible with every app built before it.
{doc}`exporting` describes how that is kept, and {doc}`how_it_works` how
an app's calls reach the firmware.

```{toctree}
:hidden:

app_build.md
how_it_works.md
exporting.md
building.md
recognizers.md
```
