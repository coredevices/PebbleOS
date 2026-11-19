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
  one per feature, such as `PBL_MICROPHONE`, and `PBL_MODDABLE` if it
  runs Moddable apps;
- the tags of resource file names: `slate`, `color`, `rect`, `mic`, `240w`,
  `240h`;
- the size limits.

What the system draws differently per platform (the status bar, the action
bar, default text sizes) is that of the nearest of Emery, Flint and Gabbro:
Gabbro if the display is round, Emery if it is rectangular and colour, Flint
otherwise.

## In the SDK

`pbl build sdk` writes the description of the build's platform to
`build/sdk/<platform>/platform.json`:

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

The app build lists the platforms of an SDK from these files. The headers
and the library are shared by the platforms of an ABI ({doc}`building`), so a
platform is added to an SDK by adding a directory with this file and nothing
else. A project picks among them by
name, with `targetPlatforms` ({doc}`app_build`).

## Flexible binaries

A binary built for a platform is right for one display, and a watch with
another display shows it in a bezel. A flexible binary is built for a
*class* instead, a display shape and colour depth such as `rect-color`, and
is meant for every watch of that class. `flexible_build()` in
`tools/pebble_sdk_platform.py` says what it is compiled with:

- `PBL_FLEXIBLE`, `PBL_COLOR` or `PBL_BW`, and `PBL_RECT` or `PBL_ROUND`.
  Shape and colour depth are the class, so `PBL_IF_ROUND_ELSE`,
  `PBL_IF_COLOR_ELSE`, `COLOR_FALLBACK`, `ACTION_BAR_WIDTH` and
  `STATUS_BAR_LAYER_HEIGHT` are the constants they are in any build.
- No `PBL_DISPLAY_WIDTH` and `PBL_DISPLAY_HEIGHT`, so a size written into
  the source does not compile. The app lays out from the bounds of its
  window.
- No `PBL_PLATFORM_<NAME>`. `PBL_PLATFORM_TYPE_CURRENT` is the platform
  the class is most like.
- No feature define, as watches of a class differ in their features. The
  app asks the watch it runs on with `platform_has_feature()`, and
  `PBL_IF_MICROPHONE_ELSE` and `PBL_IF_HEALTH_ELSE` do so too: both of
  their arguments are compiled, and they are no longer constants.
  `PBL_IF_SMARTSTRAP_ELSE` stays the constant it is without the define.
- The define of each feature the binary *requires*. Such a binary is only
  for watches that have the feature, so there it is the constant it is in a
  build for a platform.

`platform_has_feature()` is answered by the firmware from its platform's
description, without a system call. It is part of the SDK since revision
116, in every build.

A flexible build has a name, its class and the resource tags of the
features it requires joined by `+`: `rect-color`, `rect-color+mic`. The
builds look it up by that name like a platform, and it is the binary's
directory in the pbw. It is given 64K for its code and heap, which every
platform gives, unless its project asks for more; its other limits are the
smallest any platform of its class in the SDK has. {doc}`app_build` says how
a project asks for flexible binaries.

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
| 0xB7 | 1 | flags: bit 0 is set in a flexible binary |

Fields are only added at the end and the length grows with them, so a reader
skips what it does not know. It also ignores the feature bits it does not
know, and takes a display shape it does not know as one it cannot show.

A flexible binary has a display width and height of zero, and the app
memory it asks for. Its features are those it *requires*: the firmware does
not launch it on a watch that lacks one of them and tells the user so, and
gives it a framebuffer of the display's size. The features of any other binary say what its platform
has and are not held against the watch. Firmware that predates the flag
cannot read a record without a display size and takes the binary to be for
its own platform.

A platform that `PebbleProcessInfoFlags` has no value for, and every
flexible binary, is
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

A flexible binary has no display size, and lists the features it requires
in place of those of a platform:

```json
"platform": {
  "version": 1,
  "name": "rect-color+mic",
  "abi": "armv7m",
  "flexible": true,
  "display": { "shape": "rect", "color": true },
  "requires": ["microphone"],
  "limits": { "app_memory": 65536 }
}
```

A reader that predates flexible binaries cannot read a display without a
size, and passes the directory over.

## What a watch says

A watch says what its own platform is like in its answer to the version
request (Pebble protocol endpoint `0x0010`), so a phone or tool can pick a
binary for a platform it has never heard of. The description follows
everything the answer held before, 155 bytes in, and capability bit 30 of
the same answer says it is there. Readers that do not know it ignore the
extra bytes.

| Offset | Size | Field |
| --- | --- | --- |
| 0 | 1 | version (1) |
| 1 | 1 | length of the description in bytes (32) |
| 2 | 1 | ABI |
| 3 | 1 | display shape |
| 4 | 2 | display width |
| 6 | 2 | display height |
| 8 | 4 | features |
| 12 | 2 | app memory the watch gives an app, in KiB |
| 14 | 1 | bits of colour of a pixel |
| 15 | 1 | reserved, zero |
| 16 | 16 | name of the platform, padded with zeros |

The fields before the name are those of `PebblePlatformInfo`, with the same
values, but big endian like the rest of the answer. Here too fields are only
added at the end, and the length grows with them. A watch whose answer has
no description is of the platform its hardware revision stands for.

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

## Which binary a watch runs

Whatever installs a pbw, the phone app or `pebble-tool`, picks one of its
binaries for the watch. It compares what the watch says of its platform
with what each directory's manifest says, by one rule.

A binary **fits** a watch when

- it is of the watch's ABI,
- its display has the watch's shape and is no larger in either direction,
- it has no colour if the watch has none, and
- it needs no more app memory than the watch gives.

A flexible binary has no display size to compare. It fits when the watch
also has every feature it requires, and a feature the installer has not
heard of is one the watch lacks. The features of a binary built for a
platform are not part of the rule: the firmware answers for hardware that
is missing. Among the binaries that fit, the best is, in this order,

1. that of the watch's own platform,
2. one for the very display of the watch,
3. a flexible one of the watch's colour depth, and of several the one that
   requires the most features,
4. one for the largest display, where a flexible binary counts as one for
   the watch's display,
5. one of the watch's colour depth,
6. that of the newest platform, where a platform the installer has not
   heard of is newer than those it has, and
7. the first by name.

Watches of the frozen platforms are not asked: they take the directories
they always took.

`tools/tests/platform_fit_cases.json` is a table of watches, binaries and
the order they come out in, which every implementation of the rule is held
to. `tools/platform_fit.py` is one, there to check the table. The native
simulator, which installs a pbw itself, goes by directory names: after the
binary of its own platform it takes a flexible one whose name lists only
features it has, then the binaries of other platforms in the order above.
