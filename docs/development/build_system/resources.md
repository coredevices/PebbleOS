# Resources

System resources are the fonts, images, animations, vibration patterns and
data blobs the firmware ships in its resource pack or links into its image.
Every one of them is declared in a YAML file next to the files it is built
from, and a per-platform file picks which of those declarations a firmware
carries.

## Layout

```text
resources/
├── platforms/      one file per platform and variant
│   ├── emery.yaml    emery.prf.yaml
│   ├── gabbro.yaml   gabbro.prf.yaml
│   └── flint.yaml    flint.prf.yaml
├── fonts/          .pbf and .ttf faces; emoji variants in bw/ and color/
├── images/         by size (25x25/, 50x50/, 80x80/) and display variant
├── animations/     SVG frame directories (rect/, round/) and rendered .pdc files
├── vibes/
├── data/           raw blobs: the globe maps, the weather icon sequence
├── recovery/       what only the recovery firmware builds
├── timeline/       the timeline icon table and layouts.json.in
├── lang/           the layout of the language pack on the filesystem
└── tzdata/         the IANA snapshot the timezone database is built from
```

A directory that holds a `resources.yaml` is a **fragment**: the YAML
declares resources, and the files it names live in that directory or
below it. Files are grouped by what they are, not by which board first
used them, and there are no per-board copies: a resource shared by the
recovery and the normal firmware is one declaration both platform files
include.

Within a fragment, files that differ per display sit in a subdirectory
named after the key that selects them (`bw/`, `color/`, `round/`, or a
platform name), under the same file name. Images are further split by
their pixel size when the name carries one. File names did not change in
the move from the old tree; renaming the `Pebble_80x80_*.svg` style names
to match their resource names is a separate change.

Most resources are used by exactly one app or service. Those will move
next to the code that uses them, as fragments of their own
([below](#resources-owned-by-an-app)); `resources/` is for what the
system shares.

## Declaring resources

A fragment's `resources.yaml` lists its resources under `media`, with the
same keys an app declares its resources with in `package.json`, so the
format is already familiar from third-party apps and the generators under
`tools/resources/` are shared with the SDK unchanged:

```yaml
# resources/images/resources.yaml
media:
  - name: ACTION_BAR_ICON_CHECK
    type: pbi8
    file: action_bar_icon_check.png
    builtin: [emery, gabbro]

  - name: CHECKBOX_ICON_CHECKED
    type: pbi8
    file:
      bw: bw/checkbox-checked.png
      color: color/checkbox-checked.png
      gabbro: gabbro/checkbox-checked.png

  - name: BLE_HRM_SHARED
    type: pdc
    file: ble_hrm_shared.svg
    config: SERVICE_BLE_HRM

# resources/animations/resources.yaml
media:
  # A directory of SVG frames is one animated PDC sequence.
  - name: MODAL_EXPAND_TO_APP_SEQUENCE
    type: pdc
    file:
      rect: rect/modal_expand_to_app
      round: round/modal_expand_to_app
    targetPlatforms: [emery, gabbro]
```

`file` is relative to the fragment's directory. The keys:

| Key | Meaning |
| --- | --- |
| `name` | The resource; `RESOURCE_ID_<name>` in C. Unique across every fragment a firmware includes. |
| `type` | `font`, `png`, `pbi`, `pbi8`, `pdc`, `raw`, `vibe`, `mo`, `js`. |
| `file` | A path, or a map from platform or display tag to a path (below). |
| `targetPlatforms` | Platforms that get the resource; all of them when absent. |
| `builtin` | `true` to link the resource into the firmware image on every platform, or the list of platforms where it is. |
| `config` | A Kconfig symbol; the resource is only built when it is enabled. |
| `aliases` | Other names resolving to the same id. |
| `characterRegex`, `characterList`, `compatibility`, `extended`, `trackingAdjust`, `pixelHeight`, `compress` | Font options, as in the SDK. They shape a face rendered from a `.ttf`; a `.pbf` is used as it is. |

A `file` map is keyed by platform name (`emery`, `gabbro`, `flint`) or by
one of the platform's display tags (`bw`, `color`, `rect`, `round`). A
platform name beats a tag, and two matching tags are an error rather than
a guess, so a resource with a colour variant and a round override names
the platform for the override, as `CHECKBOX_ICON_CHECKED` does above.

Platform differences are expressed in one place, the entry itself:
`targetPlatforms` for a resource some platforms do not have, `builtin`
for the ones a platform keeps in flash, `config` for a resource a feature
brings along. Comments are comments. Any other key is an error.

Besides `media`, a fragment may carry the two tables that are not
resources themselves: `timeline`, the published timeline icon ids
(`resources/timeline/resources.yaml`), and `files`, the layout of the
language pack written to the filesystem (`resources/lang/resources.yaml`).

## Platform files

`resources/platforms/<platform>.yaml` is the normal firmware's resource
set for a platform, and `<platform>.prf.yaml` the recovery firmware's. A
platform file lists the fragments it includes, in the order their ids are
assigned, and the values the variant forces on every entry:

```yaml
# resources/platforms/flint.yaml
include:
  - fonts
  - images
  - animations
  - vibes
  - data
  - timeline
  - lang
```

```yaml
# resources/platforms/flint.prf.yaml
overrides:
  builtin: true      # PRF has no resource pack
  extended: false
include:
  - path: fonts
    only:
      - GOTHIC_18_BOLD
      - GOTHIC_24_BOLD
      - GOTHIC_14
      - GOTHIC_24
  - path: images
    only:
      - ACTION_BAR_ICON_CHECK
      - ACTION_BAR_ICON_X
      - BT_PAIR_SUCCESS
      - BT_PAIR_FAILURE
      - BT_PAIR_APPROVE_ON_PHONE
      - BT_PAIR_CONFIRMATION
      - GENERIC_WARNING_LARGE
  - recovery
```

An include is a fragment path, or a mapping with the path and an `only`
list naming the entries to take from it. That is how the recovery
firmware shares the pairing dialogs and a few fonts with the normal one
without a copy of their declarations; what only it builds, the low power
screens and the pairing font, is the `recovery` fragment. A name in an
`only` list that the fragment does not declare is an error.

Whole features a platform does not have are left out of its `include`
list; single resources use `targetPlatforms`. The rule that every
recovery resource is built in is the `overrides` block, not a special
case in the build.

Boards do not appear anywhere under `resources/`. The build picks the
platform file from `CONFIG_PLATFORM_*` and the variant, so `obelix`,
`qemu_emery` and `native_emery` build the same resources by construction.

## Resource ids

Ids are assigned in order: the platform file's fragments in `include`
order, their entries in file order, then the fragments registered from
CMake in the order their directories are added, then the resources other
build steps produce (stored apps, the timezone database). Resources stored
in the pack come before the built-in ones, then the ones declared on the
filesystem.

Ids are an artefact of one build. The firmware and its pack are produced
and flashed together, timeline ids are explicit in the `timeline` table,
and fonts are looked up by key, so nothing persists a numeric id. The
weather app's `resource_ids.pin.h` records what happened the one time a
build relied on the numbers.

## Resources owned by an app

Most resources are used by exactly one directory: the weather app is the
only user of about 50, settings of 26, workout of 18, the launcher of 11,
and a dozen smaller owners of a handful each. Those belong with the code,
the way a third-party app keeps its icons in its own `resources/`
directory.

An app fragment is the same thing as a fragment under `resources/`, in
the app's directory:

```text
fw/apps/system/workout/
├── resources/
│   ├── resources.yaml
│   ├── detected.svg
│   ├── run_tiny.svg
│   └── ...
├── workout.c
└── ...
```

and is registered from the directory's `CMakeLists.txt`, next to the
sources it serves:

```cmake
pbl_library()
pbl_library_sources(${source})
pbl_resource_fragment(workout/resources)
```

`pbl_resource_fragment(dir)` appends the directory to a global property,
and `pbl_resources()`, which runs once after every directory has been
added, hands the list to the resource build after the platform file's
fragments. A fragment is dropped with its code:
`pbl_resource_fragment_ifdef(CONFIG_SERVICE_BLE_HRM ble_hrm/resources)`
mirrors `pbl_library_sources_ifdef()` and leaves the pack with the
feature.

Only resources with a single owner move. A resource two apps share, or
one the timeline table names, stays under `resources/`, and a fragment
cannot re-declare a name another one already uses: duplicates are an
error, not an override. The moves themselves are still to come, one app
at a time.

## Build integration

`tools/cmake/resource_tree.py` resolves a platform file and the registered
fragments into the list of entries the generators consume, with `file`
chosen for the platform and made relative to `resources/`, so
`definitions_from_dict()` and the generators, which the SDK shares, are
untouched. `tools/cmake/resources.py manifest` takes `--platform-file`
and a list of `--fragment` directories. The `.reso` of a resource lands
under `<build>/<path of its source relative to the repository>.<NAME>.reso`,
which also works for fragments outside `resources/`.

Every YAML file read is added to `CMAKE_CONFIGURE_DEPENDS`. The loader
also checks the whole tree on every configure, included fragments or
not: a `file` or `characterList` that does not exist is an error, and so
is a file under a fragment that no entry names, unless it is a licence,
a `README` or a `.in` template, so the tree cannot grow orphans again.
