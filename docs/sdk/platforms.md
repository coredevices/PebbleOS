# Platforms

A platform is what apps are built against. It is a name and a description:

| Part | Values |
| --- | --- |
| Display | width and height; rectangular or round; colour (8-bit) or black and white (1-bit) |
| Features | `microphone`, `smartstrap`, `smartstrap_power`, `health`, `compass`, `touch`, `rgb_backlight`, `speaker` |
| Limits | what an app may take: code and memory, worker memory, resources, the largest font glyph |
| ABI | what app binaries are compiled for; `armv7m` is Cortex-M3 Thumb code with soft float |

The name labels the platform's directory in the SDK and in a `.pbw`.
Everything the builds need to know of a platform follows from its
description.

## Where platforms are defined

The platform is the firmware's: nothing below it (the kernel, the drivers,
a board) knows of it, and another application built on PebbleOS has none.
`pebble/Kconfig.platform` lists the platforms and declares what a platform
is described by. Each platform states its own values in
`pebble/platforms/Kconfig.<name>`. The firmware picks one for each board it
runs on, in its settings for the board, `pebble/boards/<board>.conf`:

```kconfig
CONFIG_PLATFORM_EMERY=y
```

Neither a board nor these settings can describe or alter a platform, so two
boards of one platform cannot differ. The frozen platforms (Aplite, Basalt, Chalk and Diorite),
which no firmware is built for, are described in
`tools/pebble_sdk_platform.py`.

## Adding a platform

Hardware that fits no platform gets one of its own: an entry in the choice
of `pebble/Kconfig.platform`,

```kconfig
config PLATFORM_SLATE
    bool "Slate"
```

and a `pebble/platforms/Kconfig.slate` with a default for each property it
has. What it leaves out keeps the default of `Kconfig.platform`: black and
white, rectangular, no feature, 64K for an app.

```kconfig
if PLATFORM_SLATE

config PLATFORM_NAME
    default "slate"

config PLATFORM_COLOR
    default y

config PLATFORM_DISPLAY_WIDTH
    default 240

config PLATFORM_DISPLAY_HEIGHT
    default 240

config PLATFORM_MICROPHONE
    default y

endif # PLATFORM_SLATE
```

The board its SDK is packaged from is added to `sdk/boards.yml`
({doc}`building`).

The same file states the RAM of its apps, `APP_RAM_4X_SEGMENT_SIZE` and
`APP_RAM_4X_RUNTIME_SIZE`. The RAM of the 2.x and 3.x apps it runs follows
from its shape and colour depth.

No tool is told about the platform. `tools/pebble_sdk_platform.py` derives
from the description what the builds read:

- the defines apps are compiled with: `PBL_PLATFORM_SLATE`, `PBL_COLOR` or
  `PBL_BW`, `PBL_RECT` or `PBL_ROUND`, `PBL_DISPLAY_WIDTH` and `_HEIGHT`,
  and one per feature, such as `PBL_MICROPHONE`;
- the tags of resource file names: `slate`, `color`, `rect`, `mic`, `240w`,
  `240h`;
- the size limits.

What the system draws differently per platform (the status bar, the action
bar, default text sizes) is that of the nearest of Emery, Flint and Gabbro:
Gabbro if the display is round, Emery if it is rectangular and colour, Flint
otherwise.

## In the SDK

`pbl build sdk` writes the description of the build's platform to
`build/sdk/<platform>/platform.json`, beside its headers and library:

```json
{
  "version": 1,
  "name": "emery",
  "abi": "armv7m",
  "display": { "width": 200, "height": 228, "shape": "rect", "color": true },
  "features": ["microphone", "smartstrap", "smartstrap_power", "health",
               "compass", "touch", "rgb_backlight", "speaker"],
  "limits": { "app_binary": 131072, "app_memory": 131072,
              "worker_memory": 10240, "resources": 1048576,
              "resources_appstore": 262144, "font_glyph": 512 },
  "moddable": true
}
```

The app build lists the platforms of an SDK from these files, so a platform
is added to an SDK by adding its directory. A project picks among them by
name, with `targetPlatforms` ({doc}`app_build`).

## What an app binary says

An app binary says what the platform it was built for is like, in a 16-byte
`PebblePlatformInfo` that follows the GNU build ID note. Neither the
`PebbleProcessInfo` header nor the note moves, so firmware that predates the
record finds both where they were. `PROCESS_INFO_HAS_PLATFORM_INFO` in the
header's flags says the record is there.

| Offset | Size | Field |
| --- | --- | --- |
| 0xA8 | 1 | version (1) |
| 0xA9 | 1 | length of the record in bytes (16) |
| 0xAA | 1 | ABI (1 is `armv7m`) |
| 0xAB | 1 | display shape: 0 rectangular, 1 round |
| 0xAC | 2 | display width |
| 0xAE | 2 | display height |
| 0xB0 | 4 | features, one bit each in the order of the list above |
| 0xB4 | 2 | app memory the binary needs, in KiB |
| 0xB6 | 1 | bits of colour of a pixel: 1 for black and white, 6 for 64 colours |
| 0xB7 | 1 | reserved, zero |

Fields are only added at the end and the length grows with them, so a reader
skips what it does not know. It also ignores the feature bits it does not
know, and takes a display shape it does not know as one it cannot show.

A platform that `PebbleProcessInfoFlags` has no value for is
`PROCESS_INFO_PLATFORM_OTHER` there. Firmware that predates the record takes
such an app to be for its own platform. Apps for the frozen platforms carry
no record: the firmware knows those platforms, and their SDK does not know
the record.

## What a pbw says

The `manifest.json` of each binary's directory in a pbw describes the
platform the binary was built for, so whoever installs the pbw can match a
binary to a watch without knowing the platform's name:

```json
"platform": {
  "version": 1,
  "name": "flint",
  "abi": "armv7m",
  "display": { "width": 144, "height": 168, "shape": "rect", "color": false },
  "features": ["microphone", "health", "compass", "speaker"],
  "limits": { "app_memory": 65536 }
}
```

It is the part of `platform.json` a binary is matched by. A directory whose
manifest has no `platform` is of the platform it is named after.

## Apps built for a smaller display

A watch runs an app built for a display of its own shape that is no larger
than its own in either direction. The app draws to a framebuffer of the size
it was built for, taken from its `PebblePlatformInfo`, or from its platform
when the binary carries none, and the compositor shows it either

- centered in a black bezel, the only way on black and white displays, or
- scaled up as far as its aspect ratio allows, with black bars beside it or
  above and below it.

On boards with `CONFIG_APP_SCALING` the user picks between the two in the
display settings. An app is not stretched, with one exception that
predates this: on Emery and Gabbro a scaled app of a legacy size (144x168,
or 180x180 on Gabbro) fills the display, as it always has. No other
platform does that. A round display only shows the sizes that have a row
table: its own and 180x180.
